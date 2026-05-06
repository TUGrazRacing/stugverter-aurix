# Motor Configuration Parameters - Fischer TI085-052-070-04B7S-07S04BE2

## Quick Reference

| Parameter | Value | Unit | Notes |
|-----------|-------|------|-------|
| **Motor Identification** |
| Model | TI085-052-070-04B7S-07S04BE2 | - | Fischer Elektromotoren |
| Pole Pairs | 4 | pairs | Configured in app_config.c |
| Winding | Star | - | 3-phase connected |
| **Electrical Parameters (@ 20°C)** |
| Stator Resistance (Rs) | 0.126 | Ω | Per phase, measured at 20°C |
| Phase Inductance (L) | 0.393 | mH | Per phase |
| D-axis Inductance (Ld) | 0.000393 | H | = phase inductance for SPM |
| Q-axis Inductance (Lq) | 0.000393 | H | Approximately equal to Ld (SPM motor) |
| PM Flux Linkage (Ψ_pm) | 0.492 | Wb | Numerically equals torque constant |
| Time Constant (τ = L/R) | 3.11 | ms | Determines current rise time |
| **Performance (Nominal - Water Cooled @ 20°C)** |
| Nominal Torque | 11.1 | Nm | Rated continuous torque |
| Nominal Speed | 13,250 | rpm | Rated continuous speed |
| Nominal Current | 22.6 | A | Rated continuous current |
| Nominal Power | 15,404 | W | Continuous power output |
| **Performance (Peak - S6 @ -10°C field weakening)** |
| Peak Torque | 29.1 | Nm | Short-term peak torque |
| Peak Speed | 11,600 | rpm | Speed at peak torque |
| Peak Current | 61 | A | Short-term peak current |
| Peak Power | 35,366 | W | Short-term peak power |
| **Speed Range** |
| Idle Speed (no-load) | 13,650 | rpm | No-load synchronous speed |
| Max Speed (field weakening) | 20,000 | rpm | Maximum operating speed |
| Max Frequency (idle/FW) | 1,333 | Hz | Modulation frequency limit |
| **DC Link & Switching** |
| Nominal DC Bus Voltage | 600 | VDC | Rated supply voltage |
| PWM Frequency | 20,000 | Hz | Configured in firmware |
| **Control Constants** |
| Torque Constant (kt) | 0.492 | Nm/A | Relates current to torque |
| Back-EMF Constant (ke) | 0.296 | Vrms/(rad/s) | Line-to-line phase-phase BEMF |
| Motor Constant (km) | 0.447 | Nm/√W | Size/efficiency metric |
| **Current Limiting (app_config.c)** |
| Nominal ID Reference | 0.0 | A | Default d-axis current (align only) |
| Nominal IQ Reference | 5.0 | A | Default torque-producing current |
| Max Iq (Speed Mode) | 8.0 | A | PI speed controller max output |
| **Decoupling Parameters (app_config.c)** |
| Decoupling Enable | false | - | Conservative default, enable after tuning |
| Iron Loss Offset | 0.0 | A | For MTPA efficiency, typically ~0 |

## Configuration in Code

### app_config.c - Motor Parameters
```c
app_config.foc.motor_polepairs = 4U;
app_config.foc.motor_rs = 0.126f;                  /* Stator resistance [Ohm] */
app_config.foc.motor_ld = 0.000393f;               /* D-axis inductance [H] */
app_config.foc.motor_lq = 0.000393f;               /* Q-axis inductance [H] */
app_config.foc.motor_psi_pm = 0.492f;              /* PM flux linkage [Wb] */
app_config.foc.motor_iron_loss_id_offset = 0.0f;   /* Iron loss compensation [A] */
app_config.foc.motor_decoupling_enable = false;    /* Start disabled, enable after tuning */
```

### PI Controller Gains (app_config.c)
```c
/* Current Controller - ID axis */
app_config.foc.pi_config_id.Kp = 0.03f;
app_config.foc.pi_config_id.Ki = 0.0008f;
app_config.foc.pi_config_id.outMax = 0.55f;
app_config.foc.pi_config_id.outMin = -0.55f;

/* Current Controller - IQ axis */
app_config.foc.pi_config_iq.Kp = 0.03f;
app_config.foc.pi_config_iq.Ki = 0.0008f;
app_config.foc.pi_config_iq.outMax = 0.55f;
app_config.foc.pi_config_iq.outMin = -0.55f;

/* Speed Controller */
app_config.foc.pi_config_speed.Kp = 0.01f;
app_config.foc.pi_config_speed.Ki = 0.0002f;
app_config.foc.pi_config_speed.outMax = 8.0f;
app_config.foc.pi_config_speed.outMin = -8.0f;
```

## Tuning Guidelines

### Current Controller Tuning
1. Disable decoupling initially: `motor_decoupling_enable = false`
2. Set small Kp/Ki values to avoid instability
3. Increase Kp gradually until you see ~10-20% overshoot on step responses
4. Increase Ki to eliminate steady-state error
5. Verify d and q axes respond independently
6. Record working Kp/Ki values

### Enabling Decoupling
1. Once current control is tuned, enable decoupling: `motor_decoupling_enable = true`
2. Expect improvements:
   - Faster transient response
   - Lower overshoot
   - Better high-speed performance
3. You may be able to increase Kp/Ki slightly for tighter control
4. Test step responses again to verify improvement

### Speed Controller Tuning
1. Current loop must be tight before tuning speed loop
2. Start with Kp = 0.005, Ki = 0.0001
3. Apply step speed command, observe settling
4. Increase Kp for faster response (if stable)
5. Increase Ki to eliminate steady-state speed error
6. Verify ramp rate is respected (2500 rpm/s)

## IntelliSense & Build Configuration

The following files have been configured:
- **.vscode/c_cpp_properties.json** - TASKING compiler include paths
- **.vscode/settings.json** - C++ extension configuration
- **.vscode/tasks.json** - Build and flash tasks

If IntelliSense still shows errors:
1. Press Ctrl+Shift+P → "C/C++: Select IntelliSense Configuration..."
2. Choose "TASKING AURIX"
3. Press Ctrl+Shift+P → "C/C++: Rescan Solutions"

## Datasheet References

For detailed specifications, see the attached motor datasheet (TI085-052-070-04B7S-07S04BE2):
- Page 1: Ratings and electrical constants
- Page 2: Mechanical data and thermal specs
- Page 3: Performance curves (speed-torque, power, losses)

## Key Notes

- **SPM Motor**: This is a Surface Permanent Magnet (SPM) motor → Ld ≈ Lq
- **No Saliency**: Cannot use reluctance torque (unlike IPM motors)
- **Temp Coeff**: Rs increases with temperature (~0.4% per °C)
- **Cooling**: Water-cooled variant allows higher continuous power
- **Field Weakening**: Capable of operation up to 20,000 rpm with negative ID current
