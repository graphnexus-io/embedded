#ifndef MINIOS_SD_DIAGNOSTICS_H
#define MINIOS_SD_DIAGNOSTICS_H

// These diagnostics mount the card only while a command is running. They do
// not provide a general filesystem API or persistent shell storage.
bool sd_initialize();
bool sd_print_info();
bool sd_run_read_write_test();

#endif
