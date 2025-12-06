# static-ip-fix

A Windows command-line tool to configure static IP addresses and DNS-over-HTTPS (DoH) encryption.

## Features

- Configure static IPv4 and IPv6 addresses
- Set up DNS servers with DNS-over-HTTPS encryption
- Support for Cloudflare (1.1.1.1) and Google (8.8.8.8) DNS
- DNS-only mode for quick DoH setup without changing IP
- Automatic rollback on failure
- Configuration file support
- Interface auto-detection

## Requirements

- Windows 10/11
- Administrator privileges (for cloudflare/google modes)
- MinGW-w64 (for building from source)

## Installation

### Pre-built Binary

Download `static-ip-fix.exe` from the [Releases](../../releases) page.

### Build from Source

```bash
# Using MinGW-w64
make

# Using MSVC
make MSVC=1
```

## Usage

```
static-ip-fix.exe [OPTIONS] <MODE>

MODES:
    cloudflare    Configure DNS with Cloudflare (1.1.1.1) + DoH
    google        Configure DNS with Google (8.8.8.8) + DoH
    status        Show current DNS encryption status

OPTIONS:
    -h, --help              Show help message
    -c, --config FILE       Load configuration from FILE
    -l, --list-interfaces   List available network interfaces
    -i, --interface NAME    Specify network interface name
    --dns-only              Only configure DNS (skip static IP setup)

IP OVERRIDE OPTIONS:
    --ipv4 ADDR             IPv4 address
    --ipv4-mask MASK        IPv4 subnet mask
    --ipv4-gateway GW       IPv4 gateway
    --ipv6 ADDR             IPv6 address
    --ipv6-prefix LEN       IPv6 prefix length
    --ipv6-gateway GW       IPv6 gateway
```

## Examples

### List available network interfaces

```bash
static-ip-fix.exe -l
```

### Quick DNS-only setup with Cloudflare

```bash
static-ip-fix.exe -i Ethernet --dns-only cloudflare
```

### Full static IP + DNS setup using config file

```bash
static-ip-fix.exe -c myconfig.ini cloudflare
```

### Check current DNS encryption status

```bash
static-ip-fix.exe -i Ethernet status
```

### Override config with CLI arguments

```bash
static-ip-fix.exe -c config.ini --ipv4 192.168.1.50 cloudflare
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
```

See `static-ip-fix.example.ini` for a complete example.

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

## Rollback

If any step fails during configuration, the tool automatically rolls back:
- Resets DNS to DHCP
- Removes DoH encryption templates

This ensures you don't end up with a half-configured network.

## Security

This tool uses secure Windows APIs:
- `CreateProcessW` for process execution (no shell injection)
- `StringCchPrintfW` for safe string formatting
- Input validation for interface names
- No plaintext password handling

## License

MIT License - see [LICENSE](LICENSE) file.

## Contributing

Issues and pull requests are welcome.
