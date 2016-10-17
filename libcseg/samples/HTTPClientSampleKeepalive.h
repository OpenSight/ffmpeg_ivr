
#ifndef HTTP_CLIENT_SAMPLE
#define HTTP_CLIENT_SAMPLE

#define HTTP_CLIENT_BUFFER_SIZE     8192

#include <stdio.h>
#include "HTTPClient.h"

typedef struct _HTTPParameters
{
    CHAR                    Uri[1024];   
    CHAR                    Uri2[1024];      
    UINT32                  Verbose;
    CHAR                    PostData[1024];
    CHAR                    PutFilePath[1024];    
} HTTPParameters;





#endif // HTTP_CLIENT_SAMPLE


