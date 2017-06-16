#include <map>
#include <iostream>
#include <fstream>
#include <cassert>
#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include "passdb.h"

void savepassDB(std::map<std::string,std::string> &m, char * filename) {
	std::ofstream myfile;
    myfile.open(filename);
	
	std::map<std::string, std::string>::iterator i = m.begin();  	
	while (i!=m.end()) {
		myfile << i->first << ":" << i->second << "\n";		
		i++;
		}
	myfile.close();	
}

void readpassDB(std::map<std::string,std::string> &m, const char * filename) {
	FILE *fptr;
	char * colon;
	char line[1025];
   // clear map???
   if ((fptr = fopen(filename,"r")) == NULL){
        printf("Error! opening file");
        exit(1);
		}
	while (!feof(fptr)) {
		fgets(line,1024,fptr);
		colon=line;
		while (*colon!='\n' && *colon!=':' && *colon!=0) colon++;
		if (*colon==':') {
		  *colon=0; colon++;
		  char * chomp=colon+strlen(colon)-1;
		while (*chomp=='\n' || *chomp=='\r') { *chomp=0; chomp--; }
		  //printf("%s || %s \n",line,colon); 
		  m[line]=colon; 
		}
	}
	fclose(fptr);
}


/*int main(int argc, char **argv)
{
  std::map<std::string, std::string> m;
  readpassDB(m,"puke");
  // check if key is present
  //if (m.find("world") != m.end())
  //  std::cout << "map contains key world!\n";
  // retrieve
  //std::cout << m["hello"] << '\n';
  //std::map<std::string, std::string>::iterator i = m.find("hello");
  //assert(i != m.end());
  //std::cout << "Key: " << i->first << " Value: " << i->second << '\n';
  savepassDB(m,"nuke");
  return 0;
} */