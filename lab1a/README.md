# Assignment 1
Submitted By: Sakshi Bansal, campus id: 9081457781

The code contains three files wis-grep.c, wis-tar.c, wis-untar.c. In the following document, I describe my implementation details of these three linux utilities.

### wis-grep

  - The  program starts out by checking argument size. If the number of terms in the program call is less than 2, it exits with status 1. 
  - The program stores the search term and begins processing the files mentioned in the arguments. It exits with status 1 upon discovering a file that doesn't exist. 
  - I use a combination of getline function (to read) and strstr() function to check if the line is present in the file. If it is it's printed out using printf. 

### wis-tar
- The  program starts out by checking argument size. If the number of terms in the program call is less than 3, it exits with status 1. 
- The program begins processing the files mentioned in the arguments. It exits with status 1 upon discovering a file that doesn't exist.
- For each file, it using strncpy function to extract the first 100 characters of the file name and writes it out to the tar file. Extra '\0' characters are added to tar file till 100 characters have been written out. 
- It gets file size using stat function and writes it to the tar file using fwrite function (binary write.)
- It then reads content of file mentioned in the arguments using getline() method and simultaneously writes the content of the file to tar file using fprintf() method.

### wis-untar
 - The  program starts out by checking argument size. If the number of terms in the program call is anything other than 2, it exits. 
 - Uses fgets() to read 100 characters from the tar file which correspond to the file name the data should be extracted to. Since, fgets() appends an extra null character at the end of the read string, 101 characaters should actually be read.
 - Reads next 8 bytes for the size of file. 
 - Uses the size of file in the previous step to read that many characters next and writes the content to a new file with the file name we had extracted from the tar file earlier. 
