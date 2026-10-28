# Installation

```bash
sudo cp vendor/uldaq/rules/50-uldaq.rules /lib/udev/rules.d/
```

```bash
sudo udevadm control --reload-rules
```

```bash
sudo udevadm trigger --subsystem-match=usb --attr-match=idVendor=09db
```
