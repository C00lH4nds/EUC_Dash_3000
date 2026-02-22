# EUC_Dash_3000
ESP32 based dashboard for BEGODE EUC

Works on Begode EUCs, tested only on Begode A2
Displays: current speed, wheel battery percentage, temperature, distance, onboard battery level
<img width="779" height="606" alt="Capture d&#39;écran 2026-02-22 011819" src="https://github.com/user-attachments/assets/3cdc9a74-6ed2-4ecc-914f-8bca851ac941" />
<img width="1070" height="578" alt="Capture d&#39;écran 2026-02-22 011900" src="https://github.com/user-attachments/assets/69f628f7-f821-43d4-afae-6f47e998cd89" />

!!!WARNING!!!
Onboard battery can burn or explode if short circuited. Pay attention to connexions and waterthightness.
You might want to add a BMS to avoid risk.
As we are messing with the wheel BLE messages there is a tiny risk to generate a cut off.

BOM:
ESP32 C3 Supermini: https://fr.aliexpress.com/item/1005009040816248.html
0.96 inch OLED screen: https://fr.aliexpress.com/item/1005006141235306.html
3.3V buck&boost converter: https://fr.aliexpress.com/item/1005008602719818.html
Battery connectors: scavenge from broken gadget or from https://fr.aliexpress.com/item/4001192853672.html
18350 battery: https://www.lepetitvapoteur.com/fr/accus/8325-accu-mxjo-18350-700mah.html
M2 screws: https://fr.aliexpress.com/item/1005006187019546.html
Screen protector: scavenge from blister package
Heatshrink tubing
Electronic wires
Cyanoacrylate glue to secure tubing on the case
Neoprene glue for screen protector

Wiring diagram:
