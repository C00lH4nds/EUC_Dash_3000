# EUC_Dash_3000
ESP32 based dashboard for BEGODE EUC.

Works only on Begode EUCs, tested only on Begode A2
Displays: current speed, wheel battery percentage, temperature, distance, onboard battery level, light status, connexion status.

<img width="779" height="606" alt="Capture d&#39;écran 2026-02-22 011819" src="https://github.com/user-attachments/assets/3cdc9a74-6ed2-4ecc-914f-8bca851ac941" />

<img width="1070" height="578" alt="Capture d&#39;écran 2026-02-22 011900" src="https://github.com/user-attachments/assets/69f628f7-f821-43d4-afae-6f47e998cd89" />

Designed to be mounted on Gyroriderz glove.

Huge thanks to WheelAndroid for publishing BLE message coding (https://github.com/Wheellog/Wheellog.Android)

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

TPU and PLA (or PETG)

Wiring diagram:

<img width="1089" height="616" alt="Capture d&#39;écran 2026-02-17 215041" src="https://github.com/user-attachments/assets/d7f55b04-a83a-4575-b6e4-422951ce6eee" />

Component placement:

<img width="385" height="708" alt="Capture d&#39;écran 2026-02-22 011713" src="https://github.com/user-attachments/assets/42d928d9-f906-40f7-83e1-04fdcfa4c8f1" />

All components should lock in place. OLED screen uses four optional screws with printed washers.

Printed case:

<img width="833" height="468" alt="Capture d&#39;écran 2026-02-22 011242" src="https://github.com/user-attachments/assets/bdd3cbe3-aec7-4629-81e3-3cbf03fc5cb5" />

Print in 0.2mm layers. No supports.
TPU parts have "TPU" prefix. Other parts can be printed in PLA or PETG.

Glue the screen protector to system case cover with neoprene glue. Make sure it is watertight.

<img width="787" height="617" alt="Capture d&#39;écran 2026-02-22 020949" src="https://github.com/user-attachments/assets/df30a405-6ee8-4a03-96b0-4724bf78a2f0" />

Mount everytinhg, put the glove on, gently heat the honeycomb part with a heatgun and bend the button box so it rests on the top of your thumb joint (it is always better than printing with supports!)

Battery goes below the top strap. Don't set it too tight.

Left button: double beep.
Middle button: ON/OFF. hold 5 seconds to turn off (deepsleep mode). Wheel connection is automatic at startup or if connexion is lost.
Right button: light ON/OFF.

Happy ride!
Subscribe to my channel: https://www.youtube.com/@OIGLabs

