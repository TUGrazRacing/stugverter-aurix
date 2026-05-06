# FOC Voltage Decoupling Implementation

## Overview
This document describes the industry-standard voltage decoupling implementation for Field Oriented Control (FOC). Decoupling improves transient response and steady-state accuracy by compensating for cross-axis coupling effects in the motor's d-q reference frame.

## Theory

### Why Decoupling?
In a real motor, the d and q axes are electromagnetically coupled:
- A change in d-axis current affects q-axis voltage through inductance
- A change in q-axis current affects d-axis voltage through inductance  
- Rotor rotation creates back-EMF that couples into the voltage equations

Without decoupling, the PI controllers fight against these cross-coupling effects, leading to:
- Slower transient response
- Higher overshoot
- Steady-state error
- Potential instability

### Decoupling Equations
The decoupled voltages are computed as:

```
v_d_decoupled = v_d + ω_elec * L_q * i_q
v_q_decoupled = v_q - ω_elec * L_d * i_d - ω_elec * Ψ_pm
```

Where:
- `v_d`, `v_q`: Desired d and q axis voltages from PI controllers [V]
- `i_d`, `i_q`: Measured d and q axis currents [A]
- `ω_elec`: Electrical angular velocity [rad/s]
- `L_d`: D-axis inductance (typically = L_leak + L_md) [H]
- `L_q`: Q-axis inductance (typically = L_leak + L_mq) [H]
- `Ψ_pm`: Permanent magnet flux linkage [Wb]

### Terms Explanation

**D-axis cross-coupling term: ω_elec * L_q * i_q**
- When q-axis current is present, it creates flux that rotates with the rotor
- This rotating flux induces voltage in the d-axis
- Compensation ensures d-current controller sees clean d-axis dynamics

**Q-axis cross-coupling term: ω_elec * L_d * i_d**
- Similarly compensates for d-axis current effect on q-axis

**Back-EMF term: ω_elec * Ψ_pm**
- At higher speeds, the permanent magnet creates back-EMF proportional to speed
- This opposes the q-axis voltage
- Compensation ensures the q-axis current controller can properly regulate torque current

## Motor Parameters

To enable decoupling, you must measure or estimate your motor parameters:

### 1. **Stator Resistance (Rs)** [Ohm]
- DC resistance of the stator windings
- Measure with multimeter or use motor nameplate
- Typical range: 0.1 - 10 Ω depending on motor size
- Not critical for basic decoupling (mainly used for iron loss comp)

### 2. **D-Axis Inductance (Ld)** [H]
- Inductance along the magnetization axis
- For SPM motors: Ld ≈ Lq (approximately equal)
- For IPM motors: Ld < Lq (salient)
- Typical range: 1 - 100 mH
- Methods to measure:
  - Lock rotor test: Apply DC, measure i-v relationship
  - AC impedance sweep: Use LCR meter or signal generator

### 3. **Q-Axis Inductance (Lq)** [H]  
- Inductance perpendicular to magnetization axis
- For SPM motors: Lq ≈ Ld
- For IPM motors: Lq significantly larger than Ld (enables reluctance torque)
- Same measurement methods as Ld

### 4. **Permanent Magnet Flux Linkage (Ψ_pm)** [Wb]
- Back-EMF voltage per rad/s
- Directly related to BEMF constant: Ke_bemf = Ψ_pm * pole_pairs
- Methods to measure:
  - **Speed test**: Spin motor at known speed, measure line-to-neutral voltage
    - Ψ_pm = V_bemf / (ω_mech * √(3/2) * pole_pairs) 
  - **Datasheet**: Often listed as "back-emf constant" or "torque constant"
  - **No-load test**: Run on voltage source, measure no-load current vs. speed

### 5. **Iron Loss Current (i_d_offset)** [A]
- Optional compensation for iron loss efficiency
- Typically a small negative value (demagnetizing direction)
- Often left at 0.0 for standard operation
- Advanced MTPA algorithms may use this for efficiency optimization
- Typical range: -0.5 to 0.5 A (depends on motor size and desired efficiency)

## Configuration

### Enable Decoupling
In your motor configuration (e.g., `app_config.c`), set:

```c
app_config.foc.motor_decoupling_enable = true;
```

### Set Motor Parameters
```c
app_config.foc.motor_polepairs = 3;              /* Your motor's pole pairs */
app_config.foc.motor_rs = 2.5f;                  /* Stator resistance [Ohm] */
app_config.foc.motor_ld = 0.015f;                /* D-axis inductance [H] */
app_config.foc.motor_lq = 0.018f;                /* Q-axis inductance [H] */
app_config.foc.motor_psi_pm = 0.12f;             /* PM flux linkage [Wb] */
app_config.foc.motor_iron_loss_id_offset = 0.0f; /* Iron loss comp [A] */
```

## Performance Tuning

### Signs of Insufficient Decoupling
- Oscillation when changing setpoints
- Sluggish acceleration/deceleration
- Coupling between d and q current changes
- Poor speed control despite good current control

### Signs of Excessive Decoupling (Wrong Parameters)
- Instability with decoupling enabled
- Oscillatory current response
- PI gains may need reducing
- Back-EMF overshooting at high speed

### Tuning Tips
1. **Disable initially**: Start with `motor_decoupling_enable = false` and tune PI gains
2. **Enable decoupling**: Switch to `motor_decoupling_enable = true`
3. **Verify parameters**: Double-check all motor parameters are reasonable
4. **Relax PI gains**: Decoupling typically allows more aggressive gains
5. **Test step response**: Apply step change to id_ref and iq_ref, observe smoothness

## Implementation Details

### File Structure
- `foc_decoupling.h`: Public API and declarations
- `foc_decoupling.c`: Decoupling computation implementation
- `foc.c`: Integration into main FOC control loop

### State Variables
- `foc_state->omega_elec`: Electrical angular velocity used for decoupling
- `foc_state->v_dq_decoupled`: Decoupled voltage for logging/debugging

### Control Flow (Closed-Loop)
1. **Clarke transform**: Convert 3-phase currents to alpha-beta
2. **Park transform**: Convert to d-q frame
3. **PI control**: Compute desired voltages
4. **Decoupling** (if enabled):
   - Calculate ω_elec from mechanical speed
   - Apply cross-coupling and back-EMF compensation
5. **Voltage limiting**: Ensure commands don't exceed DC-link voltage
6. **Inverse Park**: Transform back to alpha-beta
7. **SVPWM**: Generate PWM duty cycles

## Validation Checklist

- [ ] Motor parameters measured or sourced from datasheet
- [ ] `motor_decoupling_enable` set correctly
- [ ] PI gains tuned without decoupling first
- [ ] No oscillation observed when enabling decoupling
- [ ] Step response smoother with decoupling enabled
- [ ] High-speed performance improved (less back-EMF coupling)
- [ ] d and q current cross-coupling minimized
- [ ] Logged `v_dq_decoupled` values look reasonable

## References

- TI: "Field Oriented Control of 3-Phase AC Motors" (SPRACT02)
- TI: "Sensorless BLDC Control" (SPRABK5)
- Krishnan, R. "Permanent Magnet Synchronous and Brushless DC Motor Drives"
- Vas, P. "Vector Control of AC Machines"

## Notes

- For **synchronous reluctance motors** (no PM), set `motor_psi_pm = 0.0f`
- For **IPM motors**, the difference `(L_q - L_d)` enables reluctance torque
- Back-EMF term is automatically disabled at very low speeds (omega < 0.1 rad/s)
- Decoupling can be disabled dynamically at runtime if needed
