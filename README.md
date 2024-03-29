marionette
==========

A simple remote execution framework using libuv.

# Description

A tcp server that is able to execute commands on based on the whitelist files and return the result of the command to the remote client.

# Overview

## Basic Components

  * Marionette Server
  * Marionette Client
  
### Marionette Server

This is a libuv based TCP server which executes the commands locally and return the result. The commands should be already whitelisted. It also accepts arguments for the commands. There is a specific protocol for padding the information, so it is better to use the provided clients to talk to the server.

### Marionette Client

TCP Client understands Marionette Server protocol and ask to execute commands and retrieve the results. As of now we have only clients in 

 - Perl
 - Python

# Installation

**TODO: Make installation better**

## Server

you need to have libuv-devel, libuv, json-c and json-c-devel (I am not sure what are the name of these packages in different OS flavors)

```make build```

This creates a binary called _marionette_ which you can use to run the server.

```sudo ./marionette```
 
_sudo_ is used because marionette will have to use _setuid_ and _setgid_ when executing commands.

## Client

Both **Perl** and **Python** libs doesn't need any dependent modules, just drop the .py and .pm files in the proper search path and it should work. (Of course you need Socket module, but it is in the core)

# Using it

## Server

Once you execute the binary (_marionette_), the server is running. But it won't do anything good (other than replying 'iamok' to command 'ruok').

To whitelist a command, you have to create whitelist files. The whitelist files will be in _/etc/marionette_

### Whitelisting Commands

A command is accepted by server if there is a file with the name of the command dropped in _/etc/marionette_ ie, to have a command *foo* to be whitelisted you have to create a file _foo_ in _/etc/marionette_

Following is how a whitelist file will look like

```json
{
    "command": "/path/to/command",
    "runas_user": "nobody",
    "runas_group": "nobody",
    "argc": x,
    "live": 0
}
```

  - **command** is the path to binary (/bin/ls, etc) that has to be executed
  - **runas_user** is the user as whom the command will be executed
  - **runas_group** group whom the command will be executed as (NOT MANDATORY)
  - **argc** is number of arguments to the command, if there is a mismatch the command won't be executed
  - **live** says whether the command is live, if set to 0 it won't be executed (default is live, NOT MANDATORY)

Command supports a single dot format like **foo.bar** where foo is the directory inside _/etc/marionette_ and bar is int file in that directory, ie _/etc/marionette/foo/bar_ is the whitelist file for command _foo.bar_ . The reason we created a dotted format is to group similar commands together, ie all the release based commands can be put under release dir.

These commands are executed upon request from the clients.

## Clients

The language specific documentation is available with the client and we plead you to read it to know all the options. The basic usage pattern is create an object and call execute on that object by passing command name and args. When object goes out of scope the socket is closed. 

Once you call execute, 4 instance variables are set which can be accessed via obj.var
  - _result_   result string from marionette server
  - _errmsg_   err message
  - _errcode_  err code
  - _runtime_  time required by execute function

**NOTE:** the _result_ variable will contain the data written to both stdout and stderr of the spawned command (if you don't want this, use a wrapper script which will redirect stderr appropriately and give path to wrapper script in the whitelist file)

```
obj = Marionette(...)
err = obj.execute(cmd_name, args...)
```

### Perl

We include pod with the Perl Marionette package. ```perldoc Marionette``` will fetch you the the doc.

```perl
  use Marionette;

  my $_m = Marionette->new(
       HOST => "127.0.0.1",     # default is 127.0.0.1
       PORT => 9990,            # default is 9990
       TIMEOUT => 60,           # default is 900
       DEBUG => 0);

  my $err_code = $_m->execute("test.echo", "-n", "foo");

  # when object gets out of scope DESTORY will do a socket close
  # so we don't have to worry about closing the socket.
```

### Python

We include docstring with the Python Marionette module. ```import Marionette; help(Marionette)``` to get the doc.

```python
        import Marionette
        
        mc = Marionette.Client(host="127.0.0.1")

        try:
          mc.execute("test.echo", "-n", "foo")
        except Marionette.MarionetteError as e:
          print e
        else:
          print mc.result
```

### Example whitelist file for _test.echo_

the whitelist for test.echo is in ```/etc/marionette/test/echo``` and content is as follows

```json
{
    "command": "/bin/echo",
    "runas_user": "nobody",
    "runas_group": "ops",
    "argc": 2
}
```

# Other Remote Execution Framework v/s Marionette

There are many open source remote execution frameworks available, among them most famous is [SALT](http://www.saltstack.com/). Marionette is NOT a replacement for any of those. This is something that works good for us.

The reasons we developed this were due to

  - lightweight (for us who uses all flavours of AMZ instance types felt SALT is heavy on low end instances)
  - we wanted the flexibility to any run binary on the remote box and there should be whitelisting of commands on the remote end (cmd.run of salt was too much for us)
  - setuid and setguid on remote command
  - simple for the users (no zmq, simple server client design)
  - decentralization of server
  - streaming output to the client
  - language agnostic (salt has too much dependency on python)
  - awesomeness of libuv

[SALT](http://www.saltstack.com/) and other frameworks are awesome, they have myriad of features.
  
# Miscellaneous

### Bugs
Possibly many, please create issues. I am sure both server and clients can be optimized a lot. Patches, pull requests, suggestions and issues are most welcome.

### Known Bugs

### Author

[Vigith Maurice](https://github.com/vigith) & Ops Team

### marionette ??

'a puppet worked from above by strings attached to its limbs' christened by [dandixw](https://github.com/dandixw)

 
