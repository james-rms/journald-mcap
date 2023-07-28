# journald-mcap
Utility for exporting journald logs to MCAP.

## Why?

If something breaks on your robot outside of your application, you'll need to read logs to find out what went wrong.
If your robot is running systemd, chances are those logs are handled by `journald`, the systemd logging subsystem.
Common situations include:

* Flaky ethernet cables will link and unlink, this will be logged by the NIC driver in the kernel
* Sudden clock changes will be logged by the `chrony` service or whatever you use to manage time synchronization
* Network connectivity issues might be traced down to `dhcpd` failing to get an IP

This utility exports those logs into an MCAP file, which can be merged with your robot stack recordings
and viewed in sync in [Foxglove Studio](https://foxglove.dev/studio). If you use [Foxglove Data Platform](https://foxglove.dev/data-platform),
you can just upload the exported MCAP alongside your robot recordings.
