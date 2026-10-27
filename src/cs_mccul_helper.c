#include <stdio.h>       // provide printf

#include "uldaq.h"

void mcculPrintError(int err)
{
 	char errMsg[ERR_MSG_LEN];
	ulGetErrMsg(err, errMsg);
	printf("Error Code: %d \n", err);
	printf("Error Message: %s \n", errMsg);   
}
