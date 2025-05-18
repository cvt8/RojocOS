cd
Change the current working directory (calls sys_chdir)
Examples:
- cd .. → go up one level
- cd /home/alice → jump to an absolute path
If the target doesn’t exist you’ll get “No such file or directory”

mkdir
Create a new directory
Example mkdir photos
Cannot create intermediate parents in one step (mkdir a/b fails); and shows an error if the name already exists.

ls
List directory entries 
Examples ls | ls /docs

cat
Dump one or several files to stdout
Examples
- cat story.txt
- cat a.txt b.txt 4096 (read the first 4 096 B of each of a and b)
Without the optional byte-count it reads at most 1 024 B.

entropy
Endless stream of 32-bit random values from the kernel RNG (sys_getrandom)
Example entropy
Runs until killed.
After ~10 000 calls the kernel pauses to ask for fresh keystroke entropy (this parameter is set in the entropy.h file, and is arbitrary here).

rm
Securely delete a file (p-rm → sys_remove). 
The per-file AES key is discarded (every value is overwritten to 0)
Example rm secret.key
Works on regular files only; once the key is gone recovery is impossible.

touch
Create an empty file and allocate its encryption key (p-touch) 
Example touch notes.txt
Fails if the file already exists; does not update timestamps.

echo
Write its arguments back to stdout
Example echo hello > greeting.txt

plane
Tiny line-editor—reads one line from the console
Example plane diary.txt then type your new first line and press Enter
Edits one line only
