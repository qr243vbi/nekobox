+++
title = "Configuration"
description = "Learn how to configure NekoBox."
weight = 2
sort_by = "weight"

[extra]
+++

NekoBox supports two proxy modes:

## System Proxy

Routes the entire system's traffic through the proxy by modifying system proxy settings. Suitable for applications like web browsers.

## TUN Mode

Creates a virtual network interface to achieve global proxying. All application network traffic will route through the proxy, including software that does not support proxy settings.

> TUN mode is recommended for more thorough proxying.

> Note: Please disable System Proxy when enabling TUN mode; both cannot be enabled simultaneously.

## Import Subscription

### Method 1: Paste directly

Press `Ctrl+V` directly on the main interface to paste the subscription link.

> After importing, the group name defaults to `Default`. Right-click the group and select `Update Subscription` (shortcut: `Ctrl+U`).

### Method 2: Import via Settings

1. Go to `Settings` → `Groups`
2. Create a new group and enter a name
3. Change the type to `Subscription`
4. Paste the subscription link into the URL field
5. Click OK to save
6. Right-click the group and select `Refresh List`

![Import Subscription](https://raw.githubusercontent.com/qr243vbi/qr243vbi_images/refs/heads/nekobox/Screenshot_1.png)

![Refresh List](https://raw.githubusercontent.com/qr243vbi/qr243vbi_images/refs/heads/nekobox/Screenshot_2.png)

If the link is correct, the proxy list will be parsed.

![Proxy List](https://raw.githubusercontent.com/qr243vbi/qr243vbi_images/refs/heads/nekobox/Screenshot_3.png)

## Testing Latency

1. Select the entire list, then right-click
2. Select `URL Test Selected Items` (shortcut: `Ctrl+Shift+S`)

![URL Test](https://raw.githubusercontent.com/qr243vbi/qr243vbi_images/refs/heads/nekobox/Screenshot_4.png)

3. Select the node with the lowest latency, right-click and select `Enable`, or simply press `Enter`

## Enable Proxy
### Configure Rules
Whether using TUN mode or System Proxy, you need to configure routing rules (using Mainland China as an example):

1. Click `Routes` → `Download Config Profile`
2. Select `Bypass China` + `Bypass China Blocked`
3. Click `Routes` → `Routing Settings` → `Routes`
4. Delete the default routing rules, keeping only the two rules you just downloaded

![Routing Settings](https://raw.githubusercontent.com/qr243vbi/qr243vbi_images/refs/heads/nekobox/Screenshot_5.png)

### Start Proxy

> TUN mode is recommended.

1. Start a node first
2. Then select a proxy mode (TUN or System Proxy)

> Note: If using TUN mode, please disable TUN first when updating the subscription, and re-enable it after the update is complete.

![Start Proxy](https://raw.githubusercontent.com/qr243vbi/qr243vbi_images/refs/heads/nekobox/Screenshot_6.png)
