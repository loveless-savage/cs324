# Remote Compilation and Execution

The purpose of these exercises is to familiarize you with remote copy,
compilation, and execution, using `scp`, `ssh`, and `tmux`.

Using the guidance from the
[remote access information page](../REMOTE_ACCESS.md), follow the instructions
to "get on to the CS network", but do *not* log on to a CS lab machine.

From a terminal on your own computer, open a terminal from which you will run
the commands specified.

(If you do not have a personal computer, you may log on to a CS lab machine and
perform these exercises.)

 1. Copy `hello.c` to the home directory of one of the BYU CS lab machines
    using `scp`:

    (Replace "username" with your BYU CS username)

    ```bash
    scp hello.c username@schizo:
    ```

 2. Log in to one of the CS machines using the following command:

    (Again, replace "username" with your BYU CS username)

    ```bash
    ssh username@schizo
    ```

 3. Run the following command:

    ```bash
    tmux
    ```

    Your screen will look similar to how it did before, but note that the shell
    instance corresponding to the prompt you are seeing is running within
    `tmux`, a terminal multiplexer.  The idea is that you can now instantiate
    other shells on the same remote machine, in different windows and panes
    displayed alongside one another, disconnect and re-connect to your `tmux`
    instance on the remote machine, and more.

    You can interact with tmux using a special command: `ctrl`+`b`. This is the
    command prefix, used before every tmux command. Without the `ctrl`+`b`, commands
    interact with the *contents* of a pane. Note that this is different than normal
    keyboard shortcuts. You must use the command prefix (`ctrl`+`b`), and then **release**
    before doing the rest of the command.

    For example: `ctrl`+`b`, **release**, `"` (double quotation mark) is a command
    to tell tmux to split the window horizontally, as seen in step 5.

 4. Type `ctrl`+`b` followed by `"` (double quotation mark).  This will split
    the window in `tmux` horizontally into two panes and create a separate
    shell instance in the lower pane.

    You can use `ctrl`+`b` and an arrow key to move the focus between open panes. The
    arrow key corresponds to the direction you intuitively wish to move focus.
    
 5. Run the following command in the newly created pane:

    ```bash
    echo hello from lower pane
    ```

 6. Type `ctrl`+`b` followed by `%` (percent sign).  This will split the lower
    pane (i.e., where your "focus" is) vertically and create a separate shell
    instance in the new one (i.e., the one on the lower right).

 7. Run the following command in the newly created pane:

    ```bash
    echo hello from lower-right pane
    ```

 8. Move the focus to the lower-left pane, and run the following command:

    ```bash
    echo hello again
    ```

 9. Move the focus to the upper pane, and run `echo upper` in the upper pane:

 10. ```bash
     echo upper
     ```

 11. In the upper pane, run the following to compile and run `hello.c`:

     ```bash
     gcc -o hello hello.c
     ./hello
     ```

 12. Type `ctrl`+`b` followed by `d` to detach from your current tmux instance.

 13. Run the following command:

     ```bash
     tmux attach
     ```

     This should reattach you to the tmux instance that you were working on
     earlier, and it should look exactly as it did before you detached.

 14. Type `exit` or `ctrl`+`d` (without the command prefix) in each of the panes
     in your `tmux` instance, to close each shell and (when the last one closes)
     the `tmux` instance itself.  `ctrl`+`d` essentially passes an end-of-file,
     so the shell knows that its input has finished--its signal to terminate!
