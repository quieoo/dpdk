#ifndef COMMON_UTILS_H_
#define COMMON_UTILS_H_

#define APP_EXIT(format, args...) do{\
	printf("APP_EXIT: ");\
	printf(format,args);\
	exit(1);\
} while(0)

#endif /* COMMON_UTILS_H_ */