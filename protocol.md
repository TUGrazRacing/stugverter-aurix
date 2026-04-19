# Communication Protocol Specification: Aurix TC387 to PC

This document outlines the communication protocol between the Ethernet inverter (Aurix TC387) and the PC-based host application (designed for Qt Quick 6.11). 

## 1. General Architecture & Properties

* **Physical Layer / Speed:** Gigabit Ethernet
* **Byte Order:** Little Endian
* **Transport Layer Distribution:**
    * **TCP:** Exclusively used for Configuration and Commands.
    * **UDP:** Exclusively used for High-Speed Data Streaming.
* **Start Frame / Magic Word:** All TCP and UDP application payloads begin with a 2-byte Start sequence: `0xAA55`.

---

## 2. TCP Protocol (Configuration & Commands)

TCP is utilized for reading configurations, writing setpoints, and requesting streams. Every TCP command packet begins with a standard header consisting of a 2-byte Start frame, a 1-byte Command Type (`CMD-Type`), and a 2-byte Payload `Length`, followed by a variable-length payload.

### 2.1 Command Types (`CMD-Type`)

| Hex Code | Command Name | Direction | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | **Dict-Req** | PC $\rightarrow$ Aurix | Request the data dictionary. |
| `0x01` | **Read-Req** | PC $\rightarrow$ Aurix | Request the value of specific registers. |
| `0x02` | **Write-Req** | PC $\rightarrow$ Aurix | Write values to specific registers. |
| `0x03` | **Stream-Req** | PC $\rightarrow$ Aurix | Configure addresses for UDP streaming. |
| `0x06` | **Update-Config** | PC $\rightarrow$ Aurix | Commit staged configuration values to the active config. |
| `0x80` | **Dict-Data** | Aurix $\rightarrow$ PC | Response containing data dictionary entries. |
| `0x81` | **Read-Data** | Aurix $\rightarrow$ PC | Response containing requested register values. |
| `0x86` | **Update-Config-Ack** | Aurix $\rightarrow$ PC | Acknowledges a committed configuration update. |
| `0x8F` | **Error-Resp** | Aurix $\rightarrow$ PC | Error response (e.g., invalid address, read-only). |

---

### 2.2 TCP Request Packet Structures (PC to Aurix)

#### Dictionary Request (`Dict-Req`)
Requests the full data dictionary from the device.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x00`  |
| Length | 2 Bytes | `0x0000` (No payload). |

#### Read Request (`Read-Req`)
Requests the current values for a dynamic list of register addresses.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x01`  |
| Length | 2 Bytes | Total size of the address payload in bytes ($n \times 2$). |
| Address 1 | 2 Bytes | First register address to read. |
| Address | 2 Bytes | Register Address. |
| Ctrl | 1 Byte | Bits 0-3 = type, bits 4-5 = access, bits 6-7 reserved. |
| Name | 16 Bytes | Space-padded ASCII register name. |
| Unit | 5 Bytes | Unit representation (space-padded ASCII). |
Writes values to a list of register addresses. The size of the data block depends on the datatype of the target register.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x02`  |
| Length | 2 Bytes | Total size of all following addresses and data in bytes. |
| Address 1 | 2 Bytes | First register address. |
| Data 1 | Variable | Data for Address 1 (Size depends on register datatype). |
| ... | ... | ... |
| Address $n$ | 2 Bytes | $n$-th register address. |
| Data $n$ | Variable | Data for Address $n$ (Size depends on register datatype). |

#### Stream Request (`Stream-Req`)
Defines the list of register addresses the Aurix should actively stream via UDP.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x03`  |
| Length | 2 Bytes | Total size of the address payload in bytes ($n \times 2$). |
| Address 1 | 2 Bytes | First register address to stream. |
| ... | ... | ... |
| Address $n$ | 2 Bytes | $n$-th register address to stream. |

---

### 2.3 TCP Response Packet Structures (Aurix to PC)

#### Dictionary Data (`Dict-Data`)
Contains the structural definition of available registers. The payload repeats for every defined register.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x80`  |
| Length | 2 Bytes | Total size of the dictionary payload in bytes ($n \times 24$). |
| **Payload Array** | **24 Bytes / Item** | **Repeats for each dictionary entry** |
| Address | 2 Bytes | Register Address. |
| Ctrl | 1 Byte | Bits 0-3 = type, bits 4-5 = access, bits 6-7 reserved. |
| Name | 16 Bytes | Space-padded ASCII register name. |
| Unit | 5 Bytes | Unit representation (space-padded ASCII). |

#### Read Data (`Read-Data`)
Contains the values corresponding to a `Read-Req`.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x81`  |
| Length | 2 Bytes | Total size of the returned data payload in bytes. |
| Data 1 | Variable | Value of the 1st requested register (Size depends on type). |
| ... | ... | ... |
| Data $n$ | Variable | Value of the $n$-th requested register (Size depends on type). |

#### Error Response (`Error-Resp`)
Indicates a failed request from the PC.
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55`  |
| CMD-Type | 1 Byte | `0x8F`  |
| Length | 2 Bytes | `0x0003` (Size of Address + Error Code). |
| Address | 2 Bytes | The register address that triggered the error. |
| Error Code| 1 Byte | `0x01` Invalid Address, `0x02` Access Denied, etc. |

---

## 3. Data Dictionary Control Byte (`ctrl`) Specifications

The 1-byte `ctrl` field in the Dictionary Data payload specifies the datatype and access rights for a given register address. 

### 3.1 Bitfield Mapping
* **Bits 0-3:** Datatype indicator.
* **Bits 4-5:** Access rights indicator.
* **Bits 6-7:** Reserved.

### 3.2 Datatype Enum (Bits 0-3)
| Hex Value | C++ / Qt Equivalent | Description |
| :--- | :--- | :--- |
| `0x0` | `uint8_t` / `quint8` | Unsigned 8-bit integer. |
| `0x1` | `int8_t` / `qint8` | Signed 8-bit integer. |
| `0x2` | `uint16_t` / `quint16` | Unsigned 16-bit integer. |
| `0x3` | `int16_t` / `qint16` | Signed 16-bit integer. |
| `0x4` | `uint32_t` / `quint32` | Unsigned 32-bit integer. |
| `0x5` | `int32_t` / `qint32` | Signed 32-bit integer. |
| `0x6` | `uint64_t` / `quint64` | Unsigned 64-bit integer. |
| `0x7` | `int64_t` / `qint64` | Signed 64-bit integer. |
| `0x8` | `float` | 32-bit floating point. |
| `0x9` | `double` | 64-bit floating point. |
| `0xA` - `0xF`| N/A | Reserved. |

### 3.3 Access Enum (Bits 4-5)
| Hex Value | Access Type | Usage Notes |
| :--- | :--- | :--- |
| `0x0` | Read Only | Data that cannot be modified by the host. |
| `0x1` | Write After Restart | Staged into shadow config and committed later. |
| `0x2` | Write Live | Applied immediately to live setpoints. |
| `0x3` | Reserved | N/A. |

### 3.4 Register Classes
The register map is organized into three logical classes:

| Class | Access | Behavior |
| :--- | :--- | :--- |
| Config | `Write After Restart` | Writes update the buffered shadow config first and only become active after a commit. |
| Setpoint | `Write Live` | Writes update the live control targets immediately. |
| State | `Read Only` | Values are sampled from the runtime state and cannot be written by the host. |

---

## 4. UDP Protocol (Streaming Data)

Once the data stream is configured using a TCP `Stream-Req`, the Aurix will broadcast UDP packets containing the live data sequence.

### UDP Stream Response (`Stream Resp.`)
| Field | Size | Value/Description |
| :--- | :--- | :--- |
| UDP Header | Standard | Standard UDP Transport Header. |
| Start | 2 Bytes | `0xAA55`. |
| Stream ID | 1 Byte | Subscription identifier. |
| Seq-Num | 2 Bytes | Sequence number to detect packet loss/ordering. |
| Timestamp | 8 Bytes | Captured once when CPU0 samples the requested register set. Little-endian tick counter. |
| Count | 1 Byte | Number of requested datapoints in the packet. |
| Data | Variable | Consecutive values in the exact order acknowledged by `Stream-Req`. |

The `Stream-Req` acknowledgment returns the stream ID, register count, and the ordered list of register addresses. The host uses that one-time layout to decode subsequent frames without guessing the mapping.

Multiple datapoints are packed into a single UDP payload whenever they fit within the maximum application payload size. If a stream snapshot does not fit, the request is rejected rather than fragmented, so the host always receives one timestamp paired with one complete value set.