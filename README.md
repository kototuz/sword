# sWord
A flashcard CLI tool powered by [kovsh](https://github.com/zone-11/kovsh)

## Get Started
> Warning! The project uses submodules! Clone with `git clone --recursive`
``` console
git clone --recursive https://github.com/zone-11/sword.git
```
Ensure that `ncurses` is installed
Build:
``` console
./build.sh
```
Run:
``` console
./sword.out -help
```


## Usage
``` console
./sword.out repo new +n <name>
./sword.out repo del +n <name>
./sword.out repo list
./sword.out repo dump +n <name>
./sword.out repo exam +n <name> [-tui]
./sword.out card new +r <repo> +l <label> +t <transcript>
./sword.out card del +r <repo> +l <label>
./sword.out card find +r <repo> +v <value> [-transcript]
```
To get help use `-help` flag after a command

You can test the program before using
``` console
./build.sh test
```
