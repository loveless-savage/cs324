# I/O Multiplexing

The purpose of this assignment is to give you hands-on experience with
I/O multiplexing.  Code is provided for an echo server that listens for clients
to connect over a TCP connection (socket of type `SOCK_STREAM`) and echoes back
their messages, one line at a time, until they disconnect.  The server is
single-threaded and uses I/O multiplexing with `select()`.  You will change it
to use nonblocking sockets for extra efficiency.


# Maintain Your Repository

 Before beginning:
 - [Mirror the class repository](../01a-hw-private-repo-mirror), if you haven't
   already.
 - [Merge upstream changes](../01a-hw-private-repo-mirror#update-your-mirrored-repository-from-the-upstream)
   into your private repository.

 As you complete the assignment:
 - [Commit changes to your private repository](../01a-hw-private-repo-mirror#commit-and-push-local-changes-to-your-private-repo).


# Preparation

 1. Read the following in preparation for this assignment:

    - Sections 12.1 - 12.2 in the book
    - `select (2)` - general overview of `select()`

    Additionally, man pages for the following are referenced throughout the
    assignment:

    - `fcntl(2)` - used to make sockets nonblocking
    - `recv(2)`, `read(2)`

 2. Run `make` to build the server `echoservers`.

 3. Start a tmux session with five panes open.  You are welcome to arrange them
    however you want, but it might be easiest to do it something like this:

    ```
    -------------------------------------
    | client 1  | client 2  | client 3  |
    -------------------------------------
    |             server                |
    -------------------------------------
    |            analysis               |
    -------------------------------------
    ```


# I/O Multiplexing

First, please note the following major sections to `echoservers.c`:

 - Server socket setup.  This is the code that prepares the listening socket,
   i.e., the one with which new client connections will be accepted with
   `accept()`.
 - Main event loop.  This is the loop that repeatedly calls `select()` and
   then handles the event(s) (file descriptor read/write availability) that
   triggered `select()` to return.  Within this loop are two main sections:
   - Handle new clients.  This code is run when a read event is associated with
     the listen socket, which means that a new connection has been made, and
     `accept()` can be called on the listen socket.  After `accept()` has been
     called, the bit in `rfds` corresponding to the file descriptor associated
     with the newly-created socket is set.  When `select()` is later called
     with `rfds`, the new file descriptor will be among those monitored for
     incoming data.

     Note that the listening socket has already been set to use nonblocking
     I/O.  That simply means that when an event is triggered for the file
     descriptor associated with the listening socket, `accept()` can be called
     repeatedly until there are no more incoming client connections pending,
     rather than going back to the beginning of the `select()` loop.
   - Handle existing clients.  This code is run when an event is associated
     with a file descriptor other than that listen socket, i.e., one that goes
     with one of the existing clients.  In this case, `recv()` is called to
     read from the socket, after which `send()` is called to return the bytes
     read back to the client over the same socket.

Now start the `echoservers` server in the "server" pane using the following
command line:

(Replace "port" with a port of your choosing, an integer between 1024 and
65535.  Use of ports with values less than 1024 require root privileges).

```bash
./echoservers port
```

In each of the first two "client" panes run the following:

(Replace "port" with the port on which the server program is now listening.)

```bash
nc localhost port
```

"localhost" is a domain name that always refers to the local system.  This is
used in the case where client and server are running on the same system.

After both are running, type some text in the first "client" pane, and press
enter.  Repeat with the second "client" pane.  In the "analysis" pane run the
following:

```bash
ps -Lo user,pid,ppid,nlwp,lwp,state,ucmd -C echoservers | grep ^$(whoami)\\\|USER
```

The `ps` command lists information about processes that currently exist on the
system.  The `-L` option tells us to show threads ("lightweight processes") as
if they were processes.  The `-o` option is used to show only the following
fields:

 - user: the username of the user running the process
 - pid: the process ID of the process
 - ppid: the process ID of the parent process
 - nlwp: the number of threads (light-weight processes) being run
 - lwp: the thread ID of the thread
 - state: the state of the process, e.g., Running, Sleep, Zombie
 - ucmd: the command executed.

While some past homework assignments required you to use the `ps` command
without a pipeline (e.g., to send its output to `grep`), `ps` has the
shortcoming that it can't simultaneously filter by both command name and user,
so the above command line is a workaround.

 1. Show the output from the `ps` command.

 2. From the `ps` output, how many (unique) processes are running and why?
    Use the PID and LWP to identify different threads or processes.

 3. From the `ps` output, how many (unique) threads are running with each
    process and why?  Use the PID and LWP to identify different threads or
    processes.

Stop each of the clients and then the server by using `ctrl`+`c` in each of the
panes.


# Nonblocking Sockets

## Motivation

To illustrate the motivation for nonblocking sockets, modify `echoservers.c` by
surrounding the line containing the `select()` statement with the following
two print statements:

```c
printf("before select()\n"); fflush(stdout);
// select() goes here...
printf("after select()\n"); fflush(stdout);
```

Now change the value of the `len` argument passed to `recv()` in
`echoservers.c` from `MAXLINE` to `1`.

With these changes, the server is forced to read from the socket one byte at a
time.

Run `make` again, then start the server in the "server" pane:

(Replace "port" with a port of your choosing.)

```bash
./echoservers port
```

In the first of the "client" panes run the following:

(Replace "port" with the port on which the server program is now listening.)

```bash
nc localhost port
```

Type "foo" in the "client" pane where `nc` is running.  Then press "Enter".
This sends four bytes to the server ("foo" plus newline).

 4. How many times did `select()` return in conjunction with receiving the
    bytes sent by the client?  Do not include the event triggered by the
    incoming client connection, before data was sent.

 5. How many total bytes have been read from the socket and echoed back to
    that client at this point?

The inefficiency here is that `select()` was called more times than was
necessary.  Each time `recv()` was called in the loop, it could have been
called again to get more bytes that were available to be read.  However, the
concern with calling `recv()` repeatedly on a blocking socket is that when the
data has all been read, the call to `recv()` blocks.

To experiment further with this, uncomment the commented code in
`echoservers.c` that is prefaced with "UNCOMMENT FOR NONBLOCKING PART 1".  This
change can be described as follows:

   The `while(1)` loop within the `else if (FD_ISSET(i, &rfds_ready))` block
   causes the program to make repeated calls to `recv()` on the (blocking)
   socket until the client has closed its end of the connection, rather than
   returning to `select()`.

Run `make` again, then start the server in the "server" pane:

(Replace "port" with a port of your choosing.)

```bash
./echoservers port
```

In the first of the "client" panes run the following:

(Replace "port" with the port on which the server program is now listening.)

```bash
nc localhost port
```

Type "foo" in the client pane where `nc` is running.  Then press "Enter".  This
sends four bytes to the server ("foo" plus newline).

 6. How many times did `select()` return in conjunction with receiving the
    bytes sent by the client?  Do not include the event triggered by the
    incoming client connection, before data was sent.

 7. How many total bytes have been read from the socket and echoed back to the
    client at this point?

In the second "client" pane run the following:

(Replace "port" with the port on which the server program is now listening.)

```bash
nc localhost port
```

Type "bar" in the second client pane where `nc` is running.  Then press
"Enter".  This sends four bytes to the server ("bar" plus newline).

 8. How many times did `select()` return in conjunction with receiving the
    bytes sent by the second client?  Do not include the event triggered by
    the incoming client connection, before data was sent.

 9. How many total bytes have been read from the socket associated with the
    second client and echoed back to that client at this point?

To help you answer the next question, do the following.  In the third "client"
pane, run the following:

(Replace "port" with some random value -- _not_ the port on which the server is
listening.)

```bash
nc localhost randomport
```

Note that the above command is supposed to fail!

 10. Referring back to the client in the _second_ "client" pane, what happened
     to the connection request initiated by the second client?

Stop the first client by using `ctrl`+`c` in its pane.

 11. What happened to the bytes from the second client "bar" that had been
     _received_ by the kernel but not yet _read_ from the socket?

Stop the remaining clients and then the server by using `ctrl`+`c` in their
respective panes.


## Implementation

At this point, you have hopefully observed that a server that only issues a
single `read()` or `recv()` on a given socket during an iteration of
`select()`, is inefficient--when there is potentially more to be read.
And you have hopefully also observed that a `while` loop with blocking sockets
is not a viable solution for this.

Let us now introduce nonblocking sockets!  Uncomment the commented code in
`echoservers.c` that is prefaced with "UNCOMMENT FOR NONBLOCKING PART 2".  This
change can be described as follows:

 - The `fcntl()` system call sets the socket corresponding to the newly
   connected client connection (`connfd`) to nonblocking. The `fcntl()`
   statement might be described as follows: "set the flags on the socket
   associated with `connfd` to whatever flags are currently set, plus the
   nonblocking flag (`O_NONBLOCK`)".

 - The `while()` loop is terminated (i.e., using `break`) under any of the
   following conditions:
   1. The client has closed its end of the connection (i.e., `recv()`
      returns 0); in this case, the server closes the socket and cleans up any
      resources associated with the client connection;
   2. There is no more data left to read from the socket's buffer at the moment
      (i.e., `recv()` returns -1, and `errno` is set to `EAGAIN` or
      `EWOULDBLOCK`); or
   3. There was an error (i.e., `recv()` returns -1, and `errno` is set to
      something _other_ than `EAGAIN` or `EWOULDBLOCK`).

   The only other case is that `recv()` returns a positive value, which means
   that something was read from the socket's buffer, and `recv()` should be
   called again.

Run `make` again, then start the server in the "server" pane:

(Replace "port" with a port of your choosing.)

```bash
./echoservers port
```

In each of the first two "client" panes run the following:

(Replace "port" with the port on which the server program is now listening.)

```bash
nc localhost port
```

Type "foo" in the first "client" pane, where `nc` is running.  Then press
"Enter".  Then type "bar" in the second "client" pane, where `nc` is running.
Then press "Enter".


 12. How many times did `select()` return in conjunction with receiving the
     bytes sent by the first client?  Do not include the event triggered by the
     incoming client connection, before data was sent.

 13. What condition triggered the termination of the `while()` loop that
     repeatedly called `recv()` on the socket associated with the client in the
     first pane?

 14. What event caused `select()` to return after the data from the socket
     associated with the client in the first pane was read?

Stop the client in the first pane by using `ctrl`+`c`.

 15. What condition triggered the termination of the `while()` loop that
     repeatedly called `recv()` on the socket associated with the client in the
     first pane?

Answer the following questions about the behaviors associated with `select()`
and nonblocking sockets.  Use the man pages for `read(2)` and `recv(2)`, the
`echoservers.c` code, and the output of both `echoservers` and `nc` to help you
answer.

 16. What does it mean when `read()` or `recv()` returns a value greater than 0
     on a blocking or nonblocking socket?

 17. What does it mean when `read()` or `recv()` returns 0 on a blocking or
     nonblocking socket?

 18. What does it mean when `read()` or `recv()` returns a value less than 0 on
     a blocking socket?

 19. Why should `errno` be checked when `read()` or `recv()` returns a value
     less than 0 on a nonblocking socket?

 20. Which values of `errno` have a special meaning for nonblocking sockets when
     `read()` or `recv()` returns a value less than 0?

Stop each of the clients and then the server by using `ctrl`+`c` in each of the
panes.

You should now observe the behavior of an efficient, concurrent echo server
that uses I/O multiplexing with `select()` and nonblocking sockets!  Note that
the server socket used for accepting new connections was already set up to use
nonblocking sockets; you simply added this behavior to the sockets
corresponding to incoming client connections as well.

Finally, revert the value in `recv()` back to `MAXLINE` from `1`.  You now
have model code to use in other projects that use `select()`.
