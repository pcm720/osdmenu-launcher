# Legacy atad driver

This is an old LBA48-capable version of atad driver taken from ps2dev/ps2sdk@b84e567772c1b085e5f50982123b7d4ce7359c7a, modified to return 0x7fffffff total sectors for LBA48 drives larger than 1TB and updated to compile on modern PS2SDK.