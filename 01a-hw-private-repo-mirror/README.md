# Mirroring the Class GitHub Repository

In this assignment, you will create a private GitHub repository that is a
_mirror_ of the official GitHub repository for the class.

The assignments for this class are hosted in the GitHub repository.  Individual
assignments are in subfolders of that repository.  The subfolder for each
assignment consists of the following:

  - *A `README.md` file* that contains the description for the assignment.  The
    `README.md` is best viewed on the GitHub site itself using a Web browser.
    Alternatively, if you use VS Code, you can view the `README.md` from within
    VS Code, by right-clicking the file and selecting the "Open Preview"
    option.
  - *Other files* that are needed for the assignment.

Creating and managing a mirror of the class repository will help you 1)
ensure that you have _all_ the files you need for an assignment, 2) keep the
files and documentation up-to-date, and 3) simultaneously version and back up
your own code.


## Configuration

The following settings have been given to you in the instructions for this
assignment.  They will be referred to throughout this document and should be
replaced appropriately with the settings you have been given.

 - *Primary Development System*: this is the _primary_ system on which you do
   your work for this class.
 - *Class Repository ("CLASS\_REPO\_PATH")*: this is the path of the class
   repository, which will be the "upstream" for your own mirrored repository.
 - *Private Repository Name ("PRIVATE\_REPO\_NAME")*: this is the name of your
   own private repository.
 - *GitHub Username ("USERNAME")*: this is your GitHub username.  If you do not
   have a GitHub account, then you will need to
   [create one](https://github.com/signup).


## Step 0: Log on to Primary Development System

To begin, log on to your Primary Development System, either directly or
remotely via SSH.

> [!NOTE]
> If your primary development system is the CS lab machines,
> [see here](../REMOTE_ACCESS.md) for more information on accessing them
> remotely.

Subsequent steps should be performed on the Primary Development System, unless
specified otherwise.


## Step 1: Register an SSH Key for Use with GitHub

These steps enable you to interact with GitHub over SSH.  If you already have
an SSH key registered with GitHub on your system, then you do not need to do
this again.

 1. Find out if you already have an SSH key to use by running the `ls` command
    as follows:

    ```bash
    ls -ltra ~/.ssh/id_*
    ```

    If you do _not_ have an SSH key on your system, then the output will look
    something like this:

    ```
    ls: cannot access '/home/user/.ssh/id_*': No such file or directory
    ```

    Otherwise, it will list the files containing the contents of the key,
    something like this:

    ```
    -rw-r--r-- 1 user group  564 Jan  7 15:35 /home/user/.ssh/id_rsa.pub
    -rw------- 1 user group 2635 Jan  7 15:35 /home/user/.ssh/id_rsa
    ```

 2. If you identified an SSH key in the previous step, then you can skip this
    step and move on to #3.

    Run the following from the command line to create an SSH public/private key
    pair:

    ```bash
    ssh-keygen
    ```

    At the following prompt, just hit enter to use the default file location:

    ```
    Enter file in which to save the key (/home/user/.ssh/id_rsa):
    ```

    Optionally enter a passphrase at the next prompt.  Entering a passphrase
    makes sure that the private key cannot be used without the passphrase.
    This is good security practice, especially for a shared machine:

    ```
    Enter passphrase (empty for no passphrase):
    Enter same passphrase again:
    ```

 3. Display the contents of your _public_ key, and copy them to your clipboard.
    The following command prints the contents to the terminal:

    ```bash
    cat ~/.ssh/id_rsa.pub
    ```

    (This assumes the name of your public key file is `id_rsa.pub`, which is
    the default.)

 4. Follow steps 2 through 8 in the
    [official documentation](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/adding-a-new-ssh-key-to-your-github-account#adding-a-new-ssh-key-to-your-account)
    to register your SSH key with your GitHub account.


## Step 2: Create a Mirrored Version of the Class Repository

This is a one-time process to create and configure your own private GitHub
repository for referencing and committing changes.  Your private repository
will also be a mirror of the upstream class repository.

 1. Create the private repository as a new repository on GitHub.  Follow steps 1
    through 6 in the
    [official documentation](https://docs.github.com/en/get-started/quickstart/create-a-repo#create-a-repository),
    adhering to the following:

    - Create the repository under your GitHub user (USERNAME), and name the
      repository using PRIVATE\_REPO\_NAME (Step 2).
    - Make sure the visibility of the repository is _private_ (Step 4).
    - Do _not_ check the box "Initialize this repository with a README" (Step 5).

 2. Double-check that:
    - your repository is _private_; and
    - your repository is _empty_.

 3. Clone the upstream class repository by running the following command in the
    terminal:

    (Substitute "CLASS\_REPO\_PATH" with the path of the upstream class
    repository.)

    ```bash
    git clone --bare https://github.com/CLASS_REPO_PATH upstream-repo
    ```

 4. Push a mirror of the upstream repository to the new, private repository,
    which you have just created:

    (Substitute "USERNAME" with your GitHub username and "PRIVATE\_REPO\_NAME"
    with the name of your private repository.)

    ```bash
    cd upstream-repo
    git push --mirror ssh://git@github.com/USERNAME/PRIVATE_REPO_NAME
    ```

 5. Remove your clone of the upstream repository.

    ```bash
    cd ../
    rm -rf upstream-repo
    ```


## Step 3: Create a Clone of Your Private Repository

This is a one-time process to create a clone of the private repository you have
created.

 1. Clone your new, private repository, which is now a mirror of the upstream
    class repository:

    (Substitute "USERNAME" with your GitHub username and "PRIVATE\_REPO\_NAME"
    with the name of your private repository.)

    ```bash
    git clone ssh://git@github.com/USERNAME/PRIVATE_REPO_NAME
    ```

 2. Add the upstream repository to your clone:

    (Substitute "PRIVATE\_REPO\_NAME" with the name of your private repository
    and "CLASS\_REPO\_PATH" with the path of the upstream class repository.)

    ```bash
    cd PRIVATE_REPO_NAME
    git remote add upstream ssh://git@github.com/CLASS_REPO_PATH
    git remote -v
    ```

    Running `git remote -v` should produce four lines of output.  The two items
    that start with "origin" should point to your private repository, i.e.,
    "USERNAME/PRIVATE\_REPO\_NAME".  The two items that start with "upstream"
    should point to the class repository, i.e., "CLASS\_REPO\_PATH".  The
    presence of these entries means that your clone is associated primarily
    with your private repo ("origin"), and secondarily to the upstream class
    repository ("upstream").


## Step 4 (Optional): Create Additional Clones of Your Private Repository

If you would like to create additional clones of your private repository on
systems other than your Primary Development System, follow the instructions for
both
[registering SSH keys](#step-1-register-an-ssh-key-for-use-with-github) and
[creating a clone](#step-3-create-a-clone-of-your-private-repository) on _that_
system.  Just remember that you will need to keep all clones
[up-to-date](#update-your-mirrored-repository-from-the-upstream)!


## Ongoing Maintenance

### Update Your Mirrored Repository from the Upstream

Throughout the semester we might update the class repository with changes
that seem appropriate.  Every time you start an assignment, and as often as you
like, please run the commands listed below to pull down the changes from the
upstream class repository and integrate them into your own private repository.
Remember that you will need to do this for any and all clones
[that you have made of your repository](#step-3-create-a-clone-of-your-private-repository).

 1. Pull down the latest changes from both your repository and the upstream:

    ```bash
    git fetch --all
    ```

 2. Stash (save aside) any uncommitted changes that you might have locally in
    your clone:

    ```bash
    git stash
    ```

 3. Merge in the changes from the upstream repository:

    ```bash
    git merge upstream/master
    ```

 4. Merge back in any uncommitted changes that were stashed:

    ```bash
    git stash pop
    ```

 5. Push out the locally merged changes to your repository:

    ```bash
    git push
    ```


### Commit and Push Local Changes to Your Private Repo

It is best practice to regularly commit your changes and push them to the
origin.  Follow the steps below every time you want to commit changes to the
clone of your repository and push them out to your private repository on
GitHub:

 1. Mark modified files as "staged" for the next commit.

    (Replace "..." with the names of any files or directories that have
    changes.)

    ```bash
    git add ...
    ```

 2. Commit files staged for commit.

    ```bash
    git commit
    ```

    That this will open an editor within which you can enter a message
    associated with your commit.

> [!TIP]
> As an alternative to running `git commit` as shown above, you can use the
> `-m` command-line option to add your message directly from the command line,
> without using an enter.  For example: `git commit -m "improve my code"`.

 3. Push your local commits to your private repository on GitHub:

    ```bash
    git push
    ```

> [!NOTE]
> The add and commit process above isn't the only way to commit your local changes.
> The offical git documentation for [`git commit`](https://git-scm.com/docs/git-commit) lists
> several other ways you can commit your local changes.
