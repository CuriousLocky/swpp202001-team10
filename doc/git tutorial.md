
This is a brief tutorial to `Git` and `GitHub` to aid the development process.

# Table of Contents
- [Table of Contents](#table-of-contents)
  - [A brief introduction to `Git`, `GitHub` and so on](#a-brief-introduction-to-git-github-and-so-on)
  - [How to install Git](#how-to-install-git)
    - [Linux](#linux)
    - [Windows](#windows)
    - [After installation](#after-installation)
  - [`Git` basics](#git-basics)
    - [How to get a `Git` repository](#how-to-get-a-git-repository)
    - [How to interact with `Git` in a `Git` repository](#how-to-interact-with-git-in-a-git-repository)
  - [How to manage branches](#how-to-manage-branches)
  - [How to interact with remote repositories](#how-to-interact-with-remote-repositories)

## A brief introduction to `Git`, `GitHub` and so on

In short, `Git` is a version control system. It will help you keep the changes to your source code, assist parallel development of different features and make you able to retrieve your previous versions of code in case you mess up. 

`GitHub` is a website that provides a platform for you to host your code, using `Git` as its version control system. Our course project utilize it to make the collaboration possible.

You can interact with `Git` in multiple ways: using the original CLI (**C**ommand **L**ine **I**nterface), using GUI  t ools like [GitHub Desktop](https://desktop.github.com/) (Only available for Windows and Mac OS) or [TortoiseGit](https://tortoisegit.org/) (Only available for Windows), or even using `GitHub` website. However, most of the GUIs only implement a *partial subset* of `Git` functionality for simplicity. The more advanced It is highly recommended (Also, the tutorial writer has never used a GUI tool.)

## How to install Git

### Linux

Open your terminal and run:

```zsh
sudo apt install git
```

Once the download finishes, you can type 
```zsh
git --version
# output should be:
#     git version 2.17.1
# or in similar form
```
to check that `Git` is successfully installed on your computer.

### Windows

You can get `Git` for Windows for sure, but for this project it is ~~highly recommended~~ necessary to use a `Unix`-like system for the sake of development. Hence, you should use WSL (Windows Subsystem for Linux) or a virtual machine to develop. Once you have your Linux subsystem / VM set, follow the instruction on how to install Git on Linux to get your `Git`.

### After installation

Typically you'll need to set up your author name and email to be used for the commits. Usually, you would like to use the `--global` flag to set config options for current user.

```zsh
git config user.name <your user name>
git config user.email <your user email>
```

There's one more thing you can configure: `user.signingkey`, which is your default GPG key to use when creating signed tags. It's equivalent the signature you write on a cheque, which endorses your identity. See [GitHub's tutorial](https://help.github.com/en/github/authenticating-to-github/adding-a-new-gpg-key-to-your-github-account) if you want to know more about it.

## `Git` basics

### How to get a `Git` repository

A `Git` repository is typically a repository containing a hidden subdirectory named `.git`, which you can treat as a "back-up" that stores snapshots of the status of your previous c

You typically obtain a `Git` repository in one of two ways:

1. You can take a local directory that is currently not under version control, and turn it into a `Git` repository, or

2. You can clone an existing `Git` repository from elsewhere.

Since we are collaborating in one project, you'll only use the second way:

```zsh
# or if you want to clone with ssh:
# git clone git@github.com:CuriousLocky/swpp202001-team10.git
git clone https://github.com/CuriousLocky/swpp202001-team10.git
```
You can assign a designated directory to which you want to clone. For exmaple, if you type 
```zsh
git clone https://github.com/CuriousLocky/swpp202001-team10.git our-project
```
then it will be cloned into a directory called `our-project`.

If not specified, it will be cloned into a directory with the same name as the remote repository (here it will be named as `swpp202001-team10`)

### How to interact with `Git` in a `Git` repository

If you want to start version-controlling existing files, you should probably begin tracking those files and do an initial commit. You can accomplish that with a few `git add` commands that specify the files you want to track, followed by a git commit:

```zsh
# Add changes in all .c files and .h to the snapshot
git add *.c
git add *.h
```
```zsh
# Create a snapshot in the version control system
# Flag -m is for specifying the commit message. If you don't use -m, git will 
# open your default editor and require you to type the commit message.
# See commit message conventions for details.
git commit -m "Implement xxx feature"
```
You might want to save your time by typing 

```zsh
git add .
```
which simply stages all of the untracked and modified files. I **highly discourage** using this way of making commits.

You should commit according to the work you've done, typically after you finish a function that implements a desired feature. The difference between each snapshot is better fewer than 50 lines, and should be as independent as possible, so that we can pick which commit to revert to easily. 

To check what changes you have made and which files are tracked or staged, type
```zsh
git status
```

To check only unstaged changes between your index and working directory, type

```zsh
git diff
```

To check the commit history, you can type 
```zsh
git log
```
which will display the entire history in default format. It will show the long hash code of each commits, the author, modification time, both the title and the contents of the commit message. 

```bash
commit 2c5c91de76f4ba94163143b0bf729a154c5400fb (HEAD -> workflow-intro, origin/workflow-intro)
Author: CuriousLocky <835322750jm37@outlook.com>
Date:   Sat May 23 10:45:09 2020 +0900

    Cleanup empty file

commit 66222c5a0a216cd9380fda1a10643b46d02f8bdc
Author: CuriousLocky <835322750jm37@outlook.com>
Date:   Sat May 23 01:06:42 2020 +0900

    Add skeleton code and usage
...
:
```

Most of the time however, we do not need this much information. The information provided by typing: 
```zsh
git log --oneline
```
should be sufficient.

## How to manage branches

Simply typing 

```zsh
git branch
```

will show you all the branches in your local working copy, and the branch you are on. For example, if it prints:

```zsh
  master
* workflow-intro
```
It means that there are two branches on your local working copy, and your are on branch `workflow-intro`.

If you want to create a new branch with branch name `new-feature`, simply type:

```zsh
git branch new-feature
```
You can switch to another branch by typing 

```zsh
git checkout new-feature
```

If you want to create a new branch called `another-new-feature`and switch to that branch immediately, you can do 

```zsh
git checkout -b another-new-feature
```



## How to interact with remote repositories

To add a new remote repository

```zsh
git remote add <name> <url>
```

```zsh
git fetch <remote> <branch>
```
For example, you can do

```zsh
git fetch origin
```

And then you can look at the remote branches that are note yet cloned to your local machine now by

```zsh
git log origin/remote-branch
```
And then you can select which branch to merge into your current branch by 
```zsh
git merge origin/remote-branch
```
Fetching before merging allows you to do some more adjustments or perform some advanced operations (like `cherry-pick`). But if you just want to sync with your remote branches, you can simply do:

```zsh
git pull <remote> <branch>
```
which is just `git fetch` + `git merge`.

If you want to push your changes to the remote repository

```zsh
git push <remote> <branch>
```

`Git` is smart. It will make some smart decision so that you can save some typing for `push` and `pull`. If you do not specify the name of the `remote` and `branch`, `Git` will push to the branch of remote `origin` which has the same name as the current branch. 

For example, if you are on branch `new-feature`, then typing `git push` will push your changes to the `new-feature` branch of remote repository `origin`.
