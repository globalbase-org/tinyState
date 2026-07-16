
#import <Foundation/Foundation.h>
//#import <UIKit/UIKit.h>
#include <string.h>
#include "ts2/c++/cDarwin_oc.h"



void darwin_getDirectoryHome(char * str,int len)
{
NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,NSUserDomainMask, YES);
NSString *nsstr = [paths objectAtIndex:0];
const char * cstr = [nsstr cStringUsingEncoding:[NSString defaultCStringEncoding]];
	strncpy(str,cstr,len);
}

void darwin_getDirectoryCaches(char * str,int len)
{
NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory,NSUserDomainMask, YES);
NSString *nsstr = [paths objectAtIndex:0];
const char * cstr = [nsstr cStringUsingEncoding:[NSString defaultCStringEncoding]];
	strncpy(str,cstr,len);
}

void darwin_getDirectoryTmp(char * str,int len)
{
NSString *nsstr = NSTemporaryDirectory();
const char * cstr = [nsstr cStringUsingEncoding:[NSString defaultCStringEncoding]];
	strncpy(str,cstr,len);
}

void darwin_getDirectoryBundle(char * str,int len)
{
NSString *bundlePath = [[NSBundle mainBundle] bundlePath];

NSString *nsstr = [[NSBundle mainBundle] bundlePath];
const char * cstr = [nsstr cStringUsingEncoding:[NSString defaultCStringEncoding]];
	strncpy(str,cstr,len);
}
