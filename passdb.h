#include <map>
#include <string>

void savepassDB(std::map<std::string,std::string> &m, char * filename);
void readpassDB(std::map<std::string,std::string> &m, const char * filename);
