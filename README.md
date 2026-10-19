# MiniRedis

A small in-memory key-value store written in C, with one twist on the usual
Redis model: instead of a single flat keyspace, keys live in a hierarchical,
filesystem-style tree. You `MKDIR` directories, `CD` into them, and `SET`/
`GET`/`DEL` keys scoped to wherever you currently are - a key named `port`
under `/config` is a different key from `port` under `/cache`.

Single-threaded, no external dependencies beyond the C standard library.

## Commands

| Command | Description |
|---|---|
| `SET <key> <value>` | Set a key to a value in the current directory |
| `GET <key>` | Get the value for a key in the current directory |
| `DEL <key>` | Delete a key from the current directory |
| `EXISTS <key>` | Check whether a key exists (`1`/`0`) |
| `MKDIR <name>` | Create a subdirectory under the current directory |
| `CD <path>` | Change directory (`CD /`, `CD ..`, `CD <name>`, or a multi-segment relative/absolute path) |
| `LS` | List the subdirectories and keys in the current directory |
| `PWD` | Print the current directory |
| `HELP` | List all commands |
| `EXIT` / `QUIT` | Quit the REPL |

## Building and running

```bash
make
./tree
```

`make` builds with `-Wall -Wextra -Werror`, so it fails loudly on anything
the compiler considers a real problem rather than silently shipping it.

Run the small built-in self-test instead of the interactive REPL with:

```bash
./tree --test
```

### Example session

```
$ ./tree
Database Server (Type 'HELP' for commands, 'EXIT' to quit)
db> SET name redis
OK
db> GET name
"redis"
db> EXISTS name
1
db> DEL name
1
db> EXIT
```

## How it's implemented

Each directory is a `Node`; each key/value pair is a `Leaf`. Both are stored
in one C union (`Tree`), linked together with `north` (parent), `west`, and
`east` pointers.

Everything is fixed-capacity where it reasonably can be (256-byte path
components) and otherwise allocated on demand with `malloc`/`free` - there's
no garbage collector, so `DEL` and `tree_cleanup()` free exactly what they
remove.
