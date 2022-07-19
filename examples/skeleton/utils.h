

#define APP_EXIT(format, ...)					\
	do {		
		printf("APP_EXIT: ");					\
		printf(format);	\
		exit(1);					\
	} while (0)
