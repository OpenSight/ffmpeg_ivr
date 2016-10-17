

///////////////////////////////////////////////////////////////////////////////
//
// Module Name:                                                                
//   Sample.c                                                                  
//                                                                             
// Abstract: Demonstrate the HTTP API usage                                    
// Author:	 Eitan Michaelson                                                  
// Platform: Win32                                                             
//
///////////////////////////////////////////////////////////////////////////////

#include "HTTPClientSampleKeepalive.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

///////////////////////////////////////////////////////////////////////////////
//
// Function     : HTTPDebug
// Purpose      : HTTP API Debugging callback
// Gets         : arguments
// Returns      : UINT32
// Last updated : 01/09/2005
//
///////////////////////////////////////////////////////////////////////////////

VOID HTTPDebug(const CHAR* FunctionName,const CHAR *DebugDump,UINT32 iLength,CHAR *DebugDescription,...) // Requested operation
{

    va_list            pArgp;
    char               szBuffer[2048];

    memset(szBuffer,0,2048);
    va_start(pArgp, DebugDescription);
    vsprintf((char*)szBuffer, DebugDescription, pArgp); //Copy Data To The Buffer
    va_end(pArgp);

    printf("%s %s %s\n", FunctionName,DebugDump,szBuffer);
}

///////////////////////////////////////////////////////////////////////////////
//
// Function     : 
// Purpose      : 
// Gets         : 
// Returns      : 
// Last updated : 01/09/2005
//
///////////////////////////////////////////////////////////////////////////////

void HTTPDumpHelp(CHAR *ExtraInfo)
{

    printf("\nUsage: HTTPClientSample_keepalive [/H:URL1] [/H2:URL2] [/D: post data] [/F: file path]\n"\
           "\n");
    printf("For example:\n HTTPClient /H:http://www.myhost.com:82 /R:www.myproxy.com:8080 \n [/V]");
    printf("\tWill get http://www.myhost.com on TCP port 82 using the myproxy.com amd basic authentication\n\n");

    if(ExtraInfo != NULL)
    {

        printf("%s\n\n",ExtraInfo);

    }
}

#define IO_BUF_SIZE     65536
INT32 HTTPSendFile(HTTP_SESSION_HANDLE pSession, CHAR *FilePath)
{
    UINT32 ret = HTTP_CLIENT_SUCCESS;
    char * buf, p;
    int buf_size = 0, sent_size;
    FILE * f;
        
    buf = (char *)malloc(IO_BUF_SIZE);
    
    f = fopen(FilePath, "rb");
    if(!f){
        ret = HTTP_CLIENT_UNKNOWN_ERROR;
        return ret;
    }

    
    while((buf_size = fread(buf, 1, IO_BUF_SIZE, f)) != 0){
        
        ret  = HTTPClientWriteData(pSession, buf, buf_size, 5);
        if(ret != HTTP_CLIENT_SUCCESS){
            break;
        }
    }
    
    return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// Function     : 
// Purpose      : 
// Gets         : 
// Returns      : 
// Last updated : 01/09/2005
//
///////////////////////////////////////////////////////////////////////////////

INT32 HTTPParseCommandLineArgs(UINT32 argc, CHAR *argv[],HTTPParameters *pClientParams)
{

    UINT32                  nArg;
    UINT32                  nResult;
    CHAR                    *pSearchPtr = NULL;
    CHAR                    PortNum[64];

    // Check for minimal input from the user
    if(argc <= 1)
    {
        HTTPDumpHelp("Error: did not get key parameters.");
        return -1;
    };

    // Parse the arguments
    for(nArg = 1;nArg < argc; nArg++)
    {

        // Did we got a request for help?
        if(strncasecmp(argv[nArg],"/?",2) == 0)
        {
            HTTPDumpHelp(NULL);
            return -1;
        }

        if(strncasecmp(argv[nArg],"/V",2) == 0)
        {
            pClientParams->Verbose = TRUE;
            continue;
        }

        // Did we got the Host name parameter
        if(strncasecmp(argv[nArg],"/H:",3) == 0)
        {
            strcpy(pClientParams->Uri,argv[nArg] + 3);
            continue;
        }
        if(strncasecmp(argv[nArg],"/H2:",3) == 0)
        {
            strcpy(pClientParams->Uri2,argv[nArg] + 4);
            continue;
        }
        // Did we got the Proxy parameter



        
        if(strncasecmp(argv[nArg],"/D:",3) == 0)
        {
            strcpy(pClientParams->PostData,argv[nArg] + 3);
            
            continue;        
        }
        if(strncasecmp(argv[nArg],"/F:",3) == 0)
        {
            strcpy(pClientParams->PutFilePath,argv[nArg] + 3);
            
            continue;        
        }        
        
    }

    // The host name is a mandatory parameter
    if(strlen(pClientParams->Uri) == 0 || strlen(pClientParams->Uri2) == 0)
    {
        HTTPDumpHelp("Error: /H or /H2 argument is missing");
        return -1;
    }
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//
// Function     : 
// Purpose      : 
// Gets         : 
// Returns      : 
// Last updated : 01/09/2005
//
///////////////////////////////////////////////////////////////////////////////




int handle_http_request(HTTP_SESSION_HANDLE pHTTP, CHAR * uri, HTTPParameters  *ClientParams)
{
    CHAR                    Buffer[HTTP_CLIENT_BUFFER_SIZE + 1];   
    INT32                   nRetCode;
    UINT32                  nSize,nTotal = 0; 
    INT32                   status_code;
    HTTP_CLIENT             HTTPClient;
    
    // Set the Verb
    do{
        
        memset(&HTTPClient,0,sizeof(HTTP_CLIENT));
        
        if(strlen(ClientParams->PostData) != 0){
            if((nRetCode = HTTPClientSetVerb(pHTTP,VerbPost)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }
        }else if(strlen(ClientParams->PutFilePath) != 0){
            if((nRetCode = HTTPClientSetVerb(pHTTP,VerbPut)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }        
        }else{
            if((nRetCode = HTTPClientSetVerb(pHTTP,VerbGet)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }            
        }
        

        // Send a request for the home page 
        if(strlen(ClientParams->PostData) != 0){ 
            if((nRetCode = HTTPClientSendRequest(pHTTP, uri, ClientParams->PostData,
                strlen(ClientParams->PostData),TRUE,5,0)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }       
        }else if(strlen(ClientParams->PutFilePath) != 0){   
            //get file size
            int ret;
            struct stat stat_buf;
            char file_size_str[32];
            memset(&stat_buf, 0, sizeof(struct stat));
            memset(file_size_str,0, 32);
            ret = stat(ClientParams->PutFilePath, &stat_buf);
            if(ret){
                nRetCode =  HTTP_CLIENT_ERROR_LONG_INPUT;
                break;                
            }
            IToA(file_size_str, (int)stat_buf.st_size);

            
            if((nRetCode = HTTPClientAddRequestHeaders(pHTTP, "Content-Length", file_size_str, 0)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }
            
            if((nRetCode = HTTPClientSendRequest(pHTTP, uri, NULL,0,FALSE,5,0)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }  
            
            //send the file content
            if((nRetCode = HTTPSendFile(pHTTP, ClientParams->PutFilePath)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }              
        
        
        }else{
            if((nRetCode = HTTPClientSendRequest(pHTTP, uri,NULL,0,FALSE,5,0)) != HTTP_CLIENT_SUCCESS)
            {
                break;
            }      
        }

        // Retrieve the the headers and analyze them
        if((nRetCode = HTTPClientRecvResponse(pHTTP,3)) != HTTP_CLIENT_SUCCESS)
        {
            break;
        }
        
        if((nRetCode = HTTPClientGetInfo(pHTTP, &HTTPClient)) != HTTP_CLIENT_SUCCESS)
        {
            break;
        }

        // Get the data until we get an error or end of stream code
        // printf("Each dot represents %d bytes:\n",HTTP_BUFFER_SIZE );
        while(nRetCode == HTTP_CLIENT_SUCCESS || nRetCode != HTTP_CLIENT_EOS)
        {
            if(nTotal >= HTTPClient.TotalResponseBodyLength){
                break;
            }
            
            // Set the size of our buffer
            nSize = HTTP_CLIENT_BUFFER_SIZE;   

            // Get the data
            nRetCode = HTTPClientReadData(pHTTP,Buffer,nSize,5,&nSize);
            nTotal += nSize;
            // Print out the results
            Buffer[nSize] = 0;
            printf("%s",Buffer);
        }

    } while(0); // Run only once
    
    printf("\n");
    if(ClientParams->Verbose == TRUE)
    {
        printf("\n\nHTTP Client terminated %d (got %d bytes, status_code %d)\n\n",
               (int)nRetCode,(int)(nTotal), HTTPClient.HTTPStatusCode);
    }   

    return nRetCode;
}

int main(int argc, CHAR *argv[])
{

    INT32                  nRetCode;
    HTTPParameters          ClientParams;
    HTTP_SESSION_HANDLE     pHTTP;



        printf("\nHTTP Client v1.0\n\n");
        // Reset the parameters structure
        memset(&ClientParams,0,sizeof(HTTPParameters));

        
        
        // Parse the user command line arguments
        nRetCode = HTTPParseCommandLineArgs(argc,argv,&ClientParams);

        if(nRetCode == -1)
        {
            // Problem while parsing command arguments
            return -1;
        }

        // Open the HTTP request handle
        pHTTP = HTTPClientOpenRequest(HTTP_CLIENT_FLAG_KEEP_ALIVE);

#ifdef _HTTP_DEBUGGING_
        HTTPClientSetDebugHook(pHTTP,&HTTPDebug);
#endif
        
    nRetCode = handle_http_request(pHTTP, ClientParams.Uri, &ClientParams);
    //HTTPClientSetConnection(pHTTP, FALSE);
    HTTPClientReset(pHTTP);
    
    sleep(10);
    //keepalive
    nRetCode = handle_http_request(pHTTP, ClientParams.Uri2, &ClientParams);   
    
    HTTPClientCloseRequest(&pHTTP);

    return nRetCode;

}
