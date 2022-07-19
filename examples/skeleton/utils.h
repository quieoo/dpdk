#ifndef COMMON_UTILS_H_
#define COMMON_UTILS_H_

#define APP_EXIT(format, ...) do{\
	printf("APP_EXIT: ");\
	printf(format);\
	exit(1);\
} while(0)

#endif /* COMMON_UTILS_H_ */