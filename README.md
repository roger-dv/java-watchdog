## Java Watchdog

### Introduction

The `java-watchdog` is a C++ program that is intended to execute in the context of a Docker container instantiation where the service to be run by the container instance is a Java program.

This `java-watchdog` program is to be invoked instead of the Java program launcher (where said launcher program is simply called `java`). When the `java-watchdog` program starts running it determines the full file path of the Java launcher program (as can be discovered via the `PATH` environment variable), proceeds to call `fork()`, then as the child process it invokes `execv()` on the java launcher program executable, and as the parent process it uses `waitpid()` to monitor the child process (as its watchdog).

If the child process (the Java program) abruptly terminates (i.e, it crashes), then the parent watchdog detects that, logs an error message to `syslog`, and terminates with a non-zero status exit code (it returns back to the Docker `containerd-shim-runc` program that instantiated the container instance).

The intent is to shield the Docker service daemon from having a Java program crash and fail to return through a normal process exit (whether with zero or non-zero status exit code).

Experience with Dremio executor Docker container instances is that if a Dremio SQL query causes an exhaustion of memory, that this may result in an abrupt crash of the java process which then has the side effect of wedging the Docker service daemon. (When the Docker service daemon becomes wedged, the only remediation is to reboot the host.)

This java watchdog program is for the purpose of insuring that there is always a normal return back to the Docker service daemon and thereby keep it stable (unwedged).

***

### Config options to `java-watchdog`

Because any command line options supplied to the `java-watchdog` program are passed on to the normal `java` launcher program, this `java-watchdog` program gets its runtime configuration options from a `config.ini` file.

It will look for a 'config.ini' file in three different directory locations (in order of precedence):

- `"${HOME}/.config/java-watchdog/"`
- current working directory
- the executing program's directory

Here are typical contents of a `config.ini` file:
```ini
[settings]
logging_level=debug
accept_ordinal=first_found
```

Currently there are only these two setting that are supported. The `logging_level` can be set to one of the usual verbosity levels:

- `trace`
- `debug`
- `info`
- `warn`
- `error`

The default is `info` verbosity level.

The `accept_ordinal` setting can be used to specify which occurrence of `java` found by searching the `PATH` environment variable should be selected for use, and can have the value of:

- `first_found`
- `second_found`
- `third_found`
- `fourth_found`
- `fifth_found`
- `sixth_found`
- `seventh_found`
- `last_found`

The default is `first_found`.

***

### Building `java-watchdog`

Use the `CMakeLists.txt` to build the `java-watchdog` program - Clion can be used on the Linux platform (this program is specifically for Linux as its intended to be used in Docker containers).

The compiler option `-std=gnu++17` has been specified.

Have used **g++ 11.3.0** for development.

***

### Docker container modification

The `java-watchdog` program will need to become part of the container image build.

*(Will proceed by using Dremio as an example Docker containerized service where the program in question is indeed a Java program.)*

For instance, the Dremio OSS community edition source code is located here:

[https://github.com/dremio/dremio-oss](https://github.com/dremio/dremio-oss)

Can follow the README build instructions to create a `dremio-*.tar.gz` image file.

The Docker container build for dremio-oss is located here:

[https://github.com/dremio/dremio-cloud-tools/tree/master/images/dremio-oss](https://github.com/dremio/dremio-cloud-tools/tree/master/images/dremio-oss)

The README instructs as to how the Dremio Docker container can be built with a supplied `dremio-*.tar.gz` image file URL.

It is in this `Dockerfile` that modifications can be made to incorporate the `java-watchdog` program into the Docker container image.

#### Modifying the Dremio `Dockerfile`

One way to incorporate the `java-watchdog` program is to place it in a directory path location that will precede the directory where `java` is located; for Dremio, `java` is located at this file path:
```sh
/usr/local/openjdk-8/bin/java
```
The Dremio container `PATH` variable is defined like so:
```sh
/usr/local/openjdk-8/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
```

As can be seen, `java` will be found in the very first directory.

The `java-watchdog` executable could be placed at and renamed to be located as this file path:
```sh
/opt/dremio/bin/java
```

The Dremio container `Entrypoint` shell script, `/opt/dremio/bin/dremio`, will get invoked when the container is instantiated to run, and if the `PATH` environment variable is defined like so:
```sh
/opt/dremio/bin:/usr/local/openjdk-8/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
```

Then the first found `java` program will be the `java-watchdog` as the executable at this file path:
```sh
/opt/dremio/bin/java
```

A `config.ini` file can likewise be added to the container image at this file path:
```sh
/opt/dremio/bin/config.ini
```

And the `accept_ordinal` setting can be set to `second_found`. That way the `java-watchdog` program will select the normal Java launcher program as found at:
```sh
/usr/local/openjdk-8/bin/java
```

### Summary

The point of all this is to head fake the `Entrypoint` shell script `/opt/dremio/bin/dremio` into invoking the `java-watchdog` program instead of the normal `java` program, but then the `java-watchdog` program fork/execs the normal `java` program to run as its child process, which it then acts as a watchdog for.