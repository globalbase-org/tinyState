

#ifdef __cplusplus
extern "C" {
#endif


void darwin_getVersion(
		char * vstr,int vlen,
		char * nstr,int nlen);
void darwin_getDirectoryHome(char * str,int len);
void darwin_getDirectoryCaches(char * str,int len);
void darwin_getDirectoryTmp(char * str,int len);
void darwin_getDirectoryBundle(char * str,int len);

			      

#ifdef __cplusplus
}
#endif

 
