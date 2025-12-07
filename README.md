# static-ip-fix

A Windows command-line tool to configure static IP addresses and DNS-over-HTTPS (DoH) encryption.

## Quick Start

```bash
# List your network interfaces
static-ip-fix.exe -l

# Enable encrypted DNS on your interface (run as Administrator)
static-ip-fix.exe -i "Ethernet" --dns-only cloudflare
```

## Features

- **Static IP Configuration** - Set IPv4 and IPv6 addresses, masks, and gateways
- **DNS-over-HTTPS (DoH)** - Encrypt DNS queries to prevent eavesdropping
- **Built-in Providers** - Cloudflare (1.1.1.1) and Google (8.8.8.8) with one command
- **Custom DNS** - Define your own DNS servers and DoH templates
- **DNS-only Mode** - Enable DoH without changing your IP configuration
- **Automatic Rollback** - Reverts changes if any step fails
- **Config File Support** - Save settings in an INI file for reuse

## Requirements

- Windows 10 or 11
- Administrator privileges (for DNS configuration)
- CMake 3.16+ and MinGW-w64 or Visual Studio 2022 (for building from source)

## Installation

### Pre-built Binary

Download `static-ip-fix.exe` from the [Releases](../../releases) page.

### Build from Source

```bash
git clone https://github.com/yourusername/static-ip-fix-windows.git
cd static-ip-fix-windows
make
```

| Command | Description |
|---------|-------------|
| `make` | Build release (MinGW) |
| `make debug` | Build with debug symbols |
| `make vs` | Build with Visual Studio 2022 |
| `make test` | Run unit tests |
| `make clean` | Remove build artifacts |

Output: `bin/static-ip-fix.exe`

## Usage

```
static-ip-fix.exe [OPTIONS] <MODE>
```

### Modes

| Mode | Description |
|------|-------------|
| `cloudflare` | Configure DNS with Cloudflare (1.1.1.1) + DoH |
| `google` | Configure DNS with Google (8.8.8.8) + DoH |
| `custom` | Configure DNS with custom servers from config file |
| `status` | Show current DNS encryption status |

### Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `-l, --list-interfaces` | List available network interfaces |
| `-i, --interface NAME` | Specify network interface name |
| `-c, --config FILE` | Load configuration from FILE |
| `--dns-only` | Only configure DNS (skip static IP setup) |

### IP Override Options

| Option | Description |
|--------|-------------|
| `--ipv4 ADDR` | IPv4 address |
| `--ipv4-mask MASK` | IPv4 subnet mask |
| `--ipv4-gateway GW` | IPv4 gateway |
| `--ipv6 ADDR` | IPv6 address |
| `--ipv6-prefix LEN` | IPv6 prefix length |
| `--ipv6-gateway GW` | IPv6 gateway |

## Examples

```bash
# List available network interfaces
static-ip-fix.exe -l

# Quick DNS-only setup with Cloudflare
static-ip-fix.exe -i Ethernet --dns-only cloudflare

# Full static IP + DNS setup using config file
static-ip-fix.exe -c myconfig.ini cloudflare

# Check current DNS encryption status
static-ip-fix.exe -i Ethernet status

# Override config file settings with CLI arguments
static-ip-fix.exe -c config.ini --ipv4 192.168.1.50 cloudflare

# Use custom DNS provider defined in config
static-ip-fix.exe -i Ethernet custom
```

## Configuration File

Create a `static-ip-fix.ini` file in the same directory as the executable:

```ini
[interface]
name = Ethernet

[ipv4]
address = 192.168.1.100
netmask = 255.255.255.0
gateway = 192.168.1.1

[ipv6]
address = 2001:db8::100
prefix = 64
gateway = fe80::1

[dns]
ipv4_servers = 1.1.1.1, 1.0.0.1
ipv6_servers = 2606:4700:4700::1111, 2606:4700:4700::1001

[doh]
template = https://cloudflare-dns.com/dns-query
autoupgrade = yes
fallback = no
```

See `static-ip-fix.example.ini` for a complete example.

The `[dns]` and `[doh]` sections are used with the `custom` mode.

### Configuration Priority

Command line arguments override config file values. This allows you to use a base config file while overriding specific settings:

```bash
# Use config file but override the interface
static-ip-fix.exe -c config.ini -i "Wi-Fi" cloudflare
```

## DNS-only Mode

If you just want to enable DoH without changing your IP configuration, use the `--dns-only` flag:

```bash
static-ip-fix.exe -i Ethernet --dns-only cloudflare
```

This will:
1. Set DNS servers to Cloudflare (1.1.1.1, 1.0.0.1, IPv6 variants)
2. Configure DoH encryption templates
3. Enable auto-upgrade with no UDP fallback

Your existing IP configuration remains unchanged.

## How It Works

DNS-over-HTTPS (DoH) encrypts your DNS queries, preventing ISPs and network observers from seeing which websites you visit. This tool configures Windows to use DoH with these settings:

- **Auto-upgrade**: Automatically uses DoH when available
- **No UDP fallback**: Refuses unencrypted DNS (strict mode)

Under the hood, the tool executes `netsh` commands to:
1. Set DNS server addresses (IPv4 and IPv6)
2. Register DoH encryption templates
3. Optionally configure static IP addresses

## Rollback

If any step fails during configuration, the tool automatically rolls back:
- Resets DNS to DHCP
- Removes DoH encryption templates

This ensures you don't end up with a half-configured network.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Access denied" error | Run as Administrator (right-click → Run as administrator) |
| Interface not found | Use `-l` to list interfaces; use exact name with quotes if it contains spaces |
| No internet after config | Verify your gateway IP matches your router; run with `--dns-only` to skip IP config |
| Status shows "NOT ENCRYPTED" | Ensure Windows is updated; DoH requires Windows 10 build 19628+ |

## Security

- Uses `CreateProcessW` for process execution (no shell injection)
- Safe string formatting with `StringCchPrintfW`
- Input validation for interface names
- No plaintext password handling

## License

MIT License - see [LICENSE](LICENSE) file.

## Contributing

Issues and pull requests are welcome on [GitHub](../../issues).
