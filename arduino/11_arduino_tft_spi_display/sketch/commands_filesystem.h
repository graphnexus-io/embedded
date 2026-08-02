#ifndef MINIOS_COMMANDS_FILESYSTEM_H
#define MINIOS_COMMANDS_FILESYSTEM_H

#include "commands.h"

CommandStatus command_fs_echo(int argc, char *argv[]);
CommandStatus command_fs_pwd(int argc, char *argv[]);
CommandStatus command_fs_cd(int argc, char *argv[]);
CommandStatus command_fs_ls(int argc, char *argv[]);
CommandStatus command_fs_cat(int argc, char *argv[]);
CommandStatus command_fs_mkdir(int argc, char *argv[]);
CommandStatus command_fs_rmdir(int argc, char *argv[]);
CommandStatus command_fs_rm(int argc, char *argv[]);
CommandStatus command_fs_mv(int argc, char *argv[]);
CommandStatus command_fs_nano(int argc, char *argv[]);

#endif
