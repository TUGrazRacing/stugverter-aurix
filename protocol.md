# Communication Protocol Specification: Aurix TC387 to PC

This document defines the Ethernet protocol currently implemented on the Aurix TC387 firmware.

## 1. General Architecture

- Physical layer: Gigabit Ethernet
- Byte order: Little-endian
- Transport split:
  - TCP: Configuration and command channel
  - UDP: High-speed data streaming
- Start frame (magic word): `0xAA55` for TCP and UDP application payloads
- Control session model: one active TCP control client at a time
- UDP destination: broadcast (`255.255.255.255`) on stream port `3040`

## 2. TCP Command Protocol

Each TCP application frame has this layout:

| Field | Size | Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55` |
| CMD-Type | 1 Byte | Command identifier |
| Length | 2 Bytes | Payload length in bytes |
| Payload | Variable | Command-specific payload |

`Length` is the payload size only (not including header).

### 2.1 Command Types

| Hex | Name | Direction | Description |
| :--- | :--- | :--- | :--- |
| `0x00` | Dict-Req | Host -> Aurix | Request dictionary entries |
| `0x01` | Read-Req | Host -> Aurix | Read register values |
| `0x02` | Write-Req | Host -> Aurix | Write register values |
| `0x03` | Stream-Req | Host -> Aurix | Configure/start UDP streaming |
| `0x04` | Stream-Stop | Host -> Aurix | Stop active stream |
| `0x06` | Update-Config | Host -> Aurix | Commit staged config values |
| `0x80` | Dict-Data | Aurix -> Host | Dictionary response chunk |
| `0x81` | Read-Data | Aurix -> Host | Read response |
| `0x82` | Write-Ack | Aurix -> Host | Write acknowledgment |
| `0x83` | Stream-Ack | Aurix -> Host | Stream subscription acknowledgment |
| `0x84` | Stream-Stop-Ack | Aurix -> Host | Stream stop acknowledgment |
| `0x86` | Update-Config-Ack | Aurix -> Host | Config commit acknowledgment |
| `0x8F` | Error-Resp | Aurix -> Host | Error response |

## 3. TCP Payload Definitions

### 3.1 Dictionary Request (`0x00`)

Request payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Group ID | 1 Byte | Logical group selector; `0xFF` requests all groups |

If `Length != 1`, the device responds with `Error-Resp (0x8F)` and error code `0x03`.

Response (`0x80`) payload is repeated dictionary records (25 bytes each):

| Field | Size | Description |
| :--- | :--- | :--- |
| Address | 2 Bytes | Register address |
| Ctrl | 1 Byte | Bits 0-3 type, bits 4-5 access, bits 6-7 reserved |
| Group ID | 1 Byte | Logical parameter grouping identifier |
| Name | 16 Bytes | Space-padded ASCII |
| Unit | 5 Bytes | Space-padded ASCII |

Dictionary data may be split into multiple `0x80` frames. Group IDs are assigned by logical subsystem/configuration area, not by datatype or access class.

### 3.2 Read Request (`0x01`)

Request payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Address 1 | 2 Bytes | First register address |
| ... | ... | ... |
| Address n | 2 Bytes | n-th register address |

Response (`0x81`) payload:
- concatenated raw register values in the exact requested order
- no address fields in response payload

### 3.3 Write Request (`0x02`)

Request payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Address 1 | 2 Bytes | First register address |
| Data 1 | Variable | Raw value bytes for address 1 |
| ... | ... | ... |
| Address n | 2 Bytes | n-th register address |
| Data n | Variable | Raw value bytes for address n |

Response (`0x82`) payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| First Address | 2 Bytes | First written address |
| Count | 1 Byte | Number of successfully written entries |

### 3.4 Stream Request (`0x03`)

Request payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Stream ID | 1 Byte | Subscription identifier chosen by host |
| Loop Divider | 1 Byte | Capture every Nth current control loop (`1` = every loop) |
| Address 1 | 2 Bytes | First register address to stream |
| ... | ... | ... |
| Address n | 2 Bytes | n-th register address |

Response (`0x83`) payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Stream ID | 1 Byte | Active stream ID |
| Count | 1 Byte | Number of streamed registers |
| Address 1..n | 2 Bytes each | Accepted register order |

### 3.5 Stream Stop (`0x04`)

Request payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Stream ID | 1 Byte | Stream identifier to stop |

Response (`0x84`) payload:

| Field | Size | Description |
| :--- | :--- | :--- |
| Stream ID | 1 Byte | Stopped stream ID |

If the requested stream ID is not active, the device responds with `Error-Resp (0x8F)` and error code `0x03`.

### 3.6 Update Config (`0x06`)

Request payload: empty (`Length = 0`)

Response (`0x86`) payload: empty (`Length = 0`)

If `Length != 0`, the device responds with `Error-Resp (0x8F)` and error code `0x03`.

## 4. Error Response (`0x8F`)

Payload format:

| Field | Size | Description |
| :--- | :--- | :--- |
| Address | 2 Bytes | Address related to the failure (`0x0000` if not specific) |
| Error Code | 1 Byte | Error reason |

Implemented error codes:
- `0x01`: Invalid Address
- `0x02`: Access Denied
- `0x03`: Other / malformed request

## 5. Dictionary Ctrl Byte

- Bits 0-3: datatype
- Bits 4-5: access class
- Bits 6-7: reserved

Access classes:
- `0x0`: Read Only (runtime/state)
- `0x1`: Write After Restart (staged config)
- `0x2`: Write Live (setpoints)

## 5.1 Dictionary Group IDs

| Group ID | Logical Group |
| :--- | :--- |
| `0x01` | ADC configuration |
| `0x02` | Current measurement configuration |
| `0x03` | PWM configuration |
| `0x04` | FOC configuration and setpoints |
| `0x05` | FOC d-axis PI configuration |
| `0x06` | FOC q-axis PI configuration |
| `0x07` | Speed PI configuration |
| `0x08` | Resolver configuration |
| `0x20` | FOC runtime state |
| `0x21` | FOC currents and voltages |
| `0x22` | PWM duty telemetry |
| `0x23` | PI runtime state |
| `0x24` | Resolver and speed telemetry |
| `0xFF` | Host request selector for all groups, not used as an entry group |

## 6. UDP Streaming Packet

After a successful `Stream-Req`, UDP packets are sent on stream port `3040`.

UDP payload format:

| Field | Size | Description |
| :--- | :--- | :--- |
| Start | 2 Bytes | `0xAA55` |
| Stream ID | 1 Byte | Stream identifier |
| Count | 1 Byte | Number of packed samples in this UDP payload |
| Samples | Variable | Repeated `Seq-Num(2) | Timestamp(8) | Data(sample bytes)` entries |

Notes:
- Data ordering exactly follows `Stream-Ack` address order.
- Sample byte length is implied by the acknowledged register list.
- One UDP packet can contain multiple captured samples from the same stream.

## 7. Host Implementation Checklist

Use this checklist when implementing the PC/host side to ensure interoperability.

### 7.1 Framing and Parsing

- Always send TCP frames as: `AA 55 | CMD | LEN_LO LEN_HI | PAYLOAD`.
- Always parse `Length` as little-endian payload byte count.
- Reject incoming frames with mismatching payload length.
- Treat all numeric fields as little-endian.

### 7.2 Command Payload Rules

- `Dict-Req (0x00)`: send one byte group selector; use `0xFF` to request all groups.
- `Read-Req (0x01)`: send one or more 16-bit addresses; payload length must be even.
- `Write-Req (0x02)`: serialize address+value pairs with exact type sizes.
- `Stream-Req (0x03)`: send `StreamID(1) + LoopDivider(1) + address list`; include at least one address.
- `Stream-Stop (0x04)`: send exactly one byte stream ID.
- `Update-Config (0x06)`: send `Length = 0`.

### 7.3 Response Handling

- Accept dictionary in multiple `Dict-Data (0x80)` chunks and concatenate records.
- Decode `Read-Data (0x81)` values using the register types from dictionary metadata.
- Validate `Write-Ack (0x82)` count against requested writes.
- Use `Stream-Ack (0x83)` returned register order to decode UDP stream values.
- On `Error-Resp (0x8F)`, inspect both address and error code.

### 7.4 Streaming and UDP

- Listen on UDP port `3040` for stream packets.
- Expect UDP payload: `AA55, streamId, sampleCount, repeated(seq(2), timestamp(8), sampleData)`.
- Track `Seq-Num` per stream ID to detect packet loss/reordering.
- Decode `Data` strictly in the order acknowledged in `Stream-Ack`.

### 7.5 Session Behavior

- Use a single active TCP control connection; opening a new one replaces the previous client.
- Be prepared for response chunking and asynchronous stream packets.
- Re-send stream subscriptions after reconnect.

## 8. Control Mode Register Conventions

The firmware exposes runtime control mode and speed setpoint as live-write setpoints:

- `CTRL_MODE` (`u8`, access `Write Live`):
  - `0`: Current control mode (host writes `ID_REF` / `IQ_REF` directly)
  - `1`: Speed control mode (Core 1 speed loop writes `IQ_REF` from speed PI)
- `SPEED_SET` (`f32`, unit `rpm`, access `Write Live`):
  - Mechanical speed setpoint used when `CTRL_MODE = 1`

`SPEED_RPM` and `SPD_IQ_REF` are provided as read-only telemetry for monitoring and streaming.
