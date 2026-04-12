# Web Flash & Setup Page — WordPress Installation Guide

This guide explains how to add the firmware flash page to your WordPress site (leemerie3d.com).

---

## What This Does

Customers visit your website, plug in their device via USB-C, click a button, and the firmware is installed directly from the browser. No Arduino IDE, no coding, no technical knowledge needed.

**Requirements for visitors:**
- Google Chrome or Microsoft Edge (desktop only)
- USB-C data cable
- The Speed Limit Display device

---

## Files Overview

```
web/
  setup-page.html          # The full page code (paste into WordPress)
  firmware/
    manifest.json           # Tells ESP Web Tools what to flash
    speed-limit-display.bin # The compiled firmware (you generate this)
```

---

## Step 1: Compile the Firmware to .bin

Before the web page can flash devices, you need a compiled firmware file.

### In Arduino IDE:
1. Open `SpeedLimitDisplay/SpeedLimitDisplay.ino`
2. Select your board: **Tools > Board > ESP32S3 Dev Module**
3. Go to **Sketch > Export Compiled Binary**
4. The .bin file will be saved in the sketch folder
5. Rename it to `speed-limit-display.bin`

### In PlatformIO:
1. Run `pio run`
2. The .bin file is in `.pio/build/esp32s3/firmware.bin`
3. Rename to `speed-limit-display.bin`

---

## Step 2: Upload Firmware Files to WordPress

You need to host the manifest.json and .bin file on your website.

### Option A: Upload via FTP/SFTP (recommended)

1. Connect to your WordPress server via FTP (FileZilla, Cyberduck, etc.)
2. Navigate to your web root (usually `/public_html/` or `/var/www/html/`)
3. Create a folder called `firmware/`
4. Upload these two files:
   - `firmware/manifest.json`
   - `firmware/speed-limit-display.bin`
5. Verify they're accessible:
   - `https://leemerie3d.com/firmware/manifest.json`
   - `https://leemerie3d.com/firmware/speed-limit-display.bin`

### Option B: Upload via WordPress File Manager plugin

1. Install the **File Manager** plugin (WP File Manager)
2. Navigate to the root of your site
3. Create a `firmware/` folder
4. Upload both files

### Important Notes:
- Your site MUST use **HTTPS** (SSL certificate). Web Serial API requires a secure connection.
- WordPress may block .bin file uploads by default. If so, add this to your theme's `functions.php`:

```php
function allow_bin_uploads($mime_types) {
    $mime_types['bin'] = 'application/octet-stream';
    $mime_types['json'] = 'application/json';
    return $mime_types;
}
add_filter('upload_mimes', 'allow_bin_uploads');
```

---

## Step 3: Create the Setup Page in WordPress

### Method 1: Custom HTML Block (easiest)

1. In WordPress admin, go to **Pages > Add New**
2. Set the title to **"Setup"** or **"Device Setup"**
3. Set the permalink/slug to `setup` (so the URL is `leemerie3d.com/setup`)
4. Click the **+** button to add a block
5. Choose **"Custom HTML"** block
6. Open `web/setup-page.html` in a text editor
7. Copy ALL the code and paste it into the Custom HTML block
8. Click **Publish**

### Method 2: Code Editor (if Custom HTML block doesn't work)

1. Create a new page titled "Setup"
2. Click the **three dots menu** (top right) > **Code Editor**
3. Paste ALL the code from `setup-page.html`
4. Switch back to Visual Editor to preview
5. Click **Publish**

### Method 3: Use a Page Builder

If you use Elementor, Divi, or similar:
1. Add an **HTML widget** or **Code block**
2. Paste the code from `setup-page.html`
3. Save and publish

---

## Step 4: Test It

1. Visit `https://leemerie3d.com/setup` in Chrome
2. You should see the professional setup page with the blue "Install" button
3. Plug in an ESP32 via USB-C
4. Click the Install button
5. Select the device from the browser popup
6. The firmware should flash successfully

### Test checklist:
- [ ] Page loads with correct styling
- [ ] Install button appears (if using Chrome/Edge)
- [ ] Browser shows "not supported" message in Safari/Firefox
- [ ] Clicking Install shows device selection popup
- [ ] Firmware flashes successfully
- [ ] Device reboots after flashing
- [ ] Page is mobile-responsive (layout adjusts on phone screens)

---

## Step 5: Update Firmware (ongoing)

When you release a new firmware version:

1. Compile the new .bin file
2. Upload it to `leemerie3d.com/firmware/speed-limit-display.bin` (overwrite the old one)
3. Update `manifest.json` with the new version number
4. Update the version badge on the webpage (`<span class="sld-version">Firmware v1.1.0</span>`)
5. That's it — existing customers can just revisit the page and re-flash

---

## Customization

### Change Colors

The page uses a blue theme. To change the primary color, find `#2563eb` in the CSS and replace it with your brand color. Key places:
- `.sld-flash-section button` — the Install button
- `.sld-step-num` — the step number circles
- Links throughout the page

### Change Support Email

Find `support@leemerie3d.com` at the bottom of the page and replace with your actual support email.

### Change API Provider

If you switch from TomTom to Google Maps API, update:
- Step 1 text (API key instructions)
- The setup portal in the firmware (different API key field)

### Add Your Logo

Add an `<img>` tag inside the `.sld-hero` div, before the `<h1>`:

```html
<img src="/wp-content/uploads/your-logo.png" alt="LeeMerie3D"
     style="height: 60px; margin-bottom: 16px;">
```

---

## Architecture Diagram

```
Customer's Computer (Chrome)
    |
    |  1. Visit leemerie3d.com/setup
    v
WordPress Site (HTTPS)
    |
    |  2. Load setup-page.html
    |  3. Load ESP Web Tools JS library
    |  4. Download manifest.json
    |  5. Download speed-limit-display.bin
    v
ESP Web Tools (in browser)
    |
    |  6. Connect to device via Web Serial API
    |  7. Flash firmware over USB
    v
ESP32 Device (USB-C)
    |
    |  8. Reboot with new firmware
    |  9. Create "SpeedLimit-Setup" WiFi hotspot
    v
Customer's Phone
    |
    |  10. Connect to hotspot
    |  11. Enter WiFi + API key on captive portal
    v
Device Ready to Use
```

---

## Security Notes

- The firmware .bin file is publicly accessible on your website. This is normal and expected — the firmware doesn't contain any secrets.
- Customer WiFi credentials and API keys are entered directly on the device via the captive portal, never through your website.
- Web Serial API only works over HTTPS, ensuring the connection is encrypted.
- The browser asks for explicit permission before connecting to any USB device.

---

## Troubleshooting the Page

| Problem | Solution |
|---------|----------|
| Install button not showing | Check browser (must be Chrome/Edge). Check console for JS errors. |
| "Failed to fetch manifest" | Verify manifest.json is at the correct URL and CORS headers allow it |
| Firmware won't download | Check .bin file URL is correct and accessible. File may be too large for hosting plan. |
| CORS errors in console | Add this to .htaccess: `Header set Access-Control-Allow-Origin "*"` |
| WordPress blocks .bin upload | Add the `allow_bin_uploads` filter to functions.php (see Step 2) |
| Page looks broken | Make sure you pasted the complete code including the `<style>` tag at the top |
