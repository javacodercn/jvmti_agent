#include <jvmti.h> 
#include <stdlib.h>
#include <pthread.h>
#include "log.h"
#include "network.h"
#include <semaphore.h>
#include "network.h"
#include <errno.h>
#include <time.h>

void startAgentThread() ;
//vm init callback
void vmInitCallback (jvmtiEnv *jvmti_env,
     JNIEnv* jni_env,
     jthread thread) ;

void enableCapabilities();

void JNICALL
FieldModificationCallBack(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jmethodID method,
            jlocation location,
            jclass field_klass,
            jobject object,
            jfieldID field,
            char signature_type,
            jvalue new_value) ;

char* gatherJvmInfoCallback(char* param, ssize_t sz);
	 
void printThread() ;
void setFieldWatch() ;
void formatValue(char signature_type, jvalue new_value, char * buf, size_t sz) ;
void printStackTrace(jthread thread);

static JavaVM *local_jvm;
static JNIEnv* jni_env;
static jvmtiEnv *jvmti_env;
static sem_t g_sem_t;
int stop = 0;
int done = 0;

#define GLOBAL_BUFFER_SIZE 1024
char g_buf[GLOBAL_BUFFER_SIZE];
char *g_pos = g_buf;

extern int log_level;

void checkResult(char* feature, jvmtiError error) {
	if(JVMTI_ERROR_NONE != error) {
		char *p =NULL;
		(*jvmti_env)->GetErrorName(jvmti_env, error, &p);
		_LOG_(LOG_ERROR,"%s : %s \n", *feature, *p);
	}	
}
	
// hook into jvm startup
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
	_LOG_(LOG_INFO,"Agent_OnLoad called\n");
	jvmtiError error;
	local_jvm = jvm;
	//jvmtiInterface_1_ *jvmtiEnv, &jvmti_env means cast jvmtiInterface_1_ *** pointer to void**,fuck design
	error = (*local_jvm)->GetEnv(local_jvm, (void **)&jvmti_env, JVMTI_VERSION_1_2);
	checkResult("GetEnv", error);

	enableCapabilities();
	
	jvmtiEventCallbacks *callbacks = (jvmtiEventCallbacks *)malloc(sizeof(jvmtiEventCallbacks));
	callbacks->VMInit = vmInitCallback;
	callbacks->FieldModification = FieldModificationCallBack;
	error = (*jvmti_env)->SetEventCallbacks(jvmti_env,callbacks,sizeof(*callbacks));
	checkResult("SetEventCallbacks", error);
	free(callbacks);

	error = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE, JVMTI_EVENT_VM_INIT,NULL);
	checkResult("SetEventNotificationMode.JVMTI_EVENT_VM_INIT", error);
	
	return JNI_OK;
}

void vmInitCallback (jvmtiEnv *jvmti_env,
     JNIEnv* jniEnv,
     jthread thread) {
		 
	_LOG_(LOG_DEBUG,"vmInitCallbacks called\n");
	jni_env = jniEnv;
	setFieldWatch();
	
	startAgentThread(jniEnv);
}

void JNICALL agentThread_proc
    (jvmtiEnv* jvmti_env, 
     JNIEnv* jni_env, 
     void* arg) {
	_LOG_(LOG_DEBUG,"agentThread_proc-------threddId:%d\n", (unsigned int)pthread_self());
	//printThread();
	
	int ret = sem_init(&g_sem_t, 0, 0);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "sem_init() error %s\n", strerror(errno));
	}
	
	ret = network_init(8000);
	if(ret == -1) {
		_LOG_(LOG_ERROR,"agent thread start failed\n");
		return ;
	}
	while(!done && network_accept(gatherJvmInfoCallback) != -1) {
	}
	network_close();
}

void startAgentThread(JNIEnv* jniEnv) {
	_LOG_(LOG_DEBUG,"%s\n", "startAgentThread---------------");
	
	jclass threadClass = (*jniEnv)->FindClass(jniEnv, "java/lang/Thread");
	//signature ref to https://docs.oracle.com/javase/7/docs/technotes/guides/jni/spec/types.html
	jmethodID threadConstructor = (*jniEnv)->GetMethodID(jniEnv, threadClass, "<init>", "()V");
	jobject agentThread = (*jniEnv)->NewObject(jniEnv, threadClass, threadConstructor);
	jvmtiError error;
	error = (*jvmti_env)->RunAgentThread(jvmti_env,
				agentThread,
				agentThread_proc,
				(void *)NULL,
				JVMTI_THREAD_NORM_PRIORITY);
	checkResult("RunAgentThread result", error);
}


void setFieldWatch() {
	_LOG_(LOG_DEBUG,"%s\n", "----start setFieldWatch ------");
	jvmtiError error = (*jvmti_env)->SetEventNotificationMode(jvmti_env, JVMTI_ENABLE , JVMTI_EVENT_FIELD_MODIFICATION,NULL);
	checkResult("SetEventNotificationMode.JVMTI_EVENT_FIELD_MODIFICATION  result", error);
	
	jclass clazz = (*jni_env)->FindClass(jni_env, "HelloWorld");
	jfieldID fieldId = (*jni_env)->GetFieldID(jni_env, clazz, "a", "I");
	error = (*jvmti_env)->SetFieldModificationWatch(jvmti_env,
            clazz,
            fieldId);
	checkResult("SetFieldModificationWatch result", error);
}

//域修改的回调函数
void JNICALL
FieldModificationCallBack(jvmtiEnv *jvmti_env,
            JNIEnv* jni_env,
            jthread thread,
            jmethodID method,
            jlocation location,
            jclass field_klass,
            jobject object,
            jfieldID field,
            char signature_type,
            jvalue new_value) {
				
	_LOG_(LOG_INFO,"FieldModificationCallBack: threadId:%d\n", (unsigned int)pthread_self());
	
	printStackTrace(thread);
	char buf[16];
	formatValue(signature_type, new_value, buf, 16);
	_LOG_(LOG_DEBUG, "formatValue() %s\n", buf);
	strncpy(g_pos, buf, strlen(buf));
	g_pos += strlen(buf);
	
	_LOG_(LOG_INFO,"FieldModificationCallBack: gatherInfo:\n%s\n", g_buf);
	
	int ret = sem_post(&g_sem_t);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "sem_post() error %s\n", strerror(errno));
	}
}


char* getClassOfMethod(jmethodID method) {
	jvmtiError error;
	char *methodName;
	error = (*jvmti_env)->GetMethodName(jvmti_env, method, 
							   &methodName, NULL, NULL);
	checkResult("GetMethodName result", error);
							   
	jclass declaring_class;
	error = (*jvmti_env)->GetMethodDeclaringClass(jvmti_env, 
                       method, 
                       &declaring_class);
	checkResult("GetMethodDeclaringClass result", error);
	
	char *signature_ptr=NULL;
	error = (*jvmti_env)->GetClassSignature(jvmti_env,
            declaring_class,
            &signature_ptr,
            NULL);
	checkResult("GetClassSignature result", error);
	_LOG_(LOG_DEBUG,"GetClassSignature  [%s][%s]\n", methodName, signature_ptr);
	
	//convert Ljava/util/List to java.util.List
	size_t sz = sizeof(char) * (strlen(signature_ptr) + strlen(methodName));
	if(sz < 1) {
		_LOG_(LOG_ERROR,"GetClassSignature length is zero!\n");
		return NULL;
	}
	char* buf = malloc(sz + 5);
	memset(buf, 0, sz + 5);
	char *p = signature_ptr + 1;
	int i =0;
	while(*p != ';' && *p!='\0') {
		buf[i++] = *p != '/' ? *p : '.';
		p++;
	}
	strcpy(buf+i,"#");
	i++;
	strcpy(buf+i, methodName);
	
	error = (*jvmti_env)->Deallocate(jvmti_env, signature_ptr);
	checkResult("GetClassSignature result", error);
	return buf;
}

void printStackTrace(jthread thread) {
	//获取调用的堆栈信息
	jvmtiFrameInfo frames[10];
	jint count;
	jvmtiError error;

	error = (*jvmti_env)->GetStackTrace(jvmti_env, thread, 0, 10, 
								   frames, &count);
	if (error == JVMTI_ERROR_NONE && count >= 1) {
	   char *signature_ptr;
	   int i =0;
	   for(i=0; i< count ; i++) {
			signature_ptr = getClassOfMethod(frames[i].method);
			int sz = strlen(signature_ptr);
			//_LOG_(LOG_DEBUG,"signature :%s\n", signature_ptr);
			if(g_pos + sz + 1 < g_buf + GLOBAL_BUFFER_SIZE) {
				 strncpy(g_pos, signature_ptr, sz);
				 strncpy(g_pos + sz, "\n", 1);
				 g_pos = g_pos + sz + 1;
				 //_LOG_(LOG_DEBUG,"g_buf:%s\n", g_buf);
			} else {
				_LOG_(LOG_ERROR,"printStackTrace overflow:%s\n", signature_ptr);
			}		   
	   }
	} else {
		checkResult("GetStackTrace result", error);
	}
}

void formatValue(char signature_type, jvalue new_value, char * buf, size_t sz) {
	//Z boolean |B byte    |C char    |S short   |
	//I int     |J long    |F float   |D double  |
	memset(buf, 0, sz);
	switch(signature_type) {
		case 'Z':
			sprintf(buf,"%s\n", new_value.z?"true":"false");
		break;
		case 'B':
			sprintf(buf,"byte %d\n", new_value.b);
		break;
		case 'C':
			buf[0]=new_value.c;
		break;
		case 'S':
			sprintf(buf,"%d\n", new_value.s);
		break;
		case 'I':
			sprintf(buf,"%d\n", new_value.i);
		case 'J':
			sprintf(buf,"%d\n", new_value.j);
		break;
		case 'F':
			sprintf(buf,"%g\n", new_value.f);
		case 'D':
			sprintf(buf,"%g\n", new_value.d);
		break;
		default:
			_LOG_(LOG_INFO,"unsupported signature_type%c\n", signature_type);
	}
}

void printThread() {
	jint threads_count_ptr;
	jthread *threads_ptr;
	jvmtiError error ;

	error = (*jvmti_env)->GetAllThreads(jvmti_env,
				&threads_count_ptr,
				&threads_ptr);
	checkResult("GetAllThreads result", error);
	_LOG_(LOG_DEBUG,"thread size:%d\n", threads_count_ptr);
	jthread *ptr = threads_ptr;
	int index = 0;
	jvmtiThreadInfo *info_ptr = malloc(sizeof(jvmtiThreadInfo));
	for(index =0; index <threads_count_ptr; index ++ ) {
		error =	(*jvmti_env)->GetThreadInfo(jvmti_env,
						*(ptr+index),
						info_ptr);
		_LOG_(LOG_INFO,"thread%d, name:%s\n", index, info_ptr->name);
	}
	free(info_ptr);
	
	error = (*jvmti_env)->Deallocate(jvmti_env, (unsigned char*)threads_ptr);
	checkResult("Deallocate result", error);
}

//确认jvmti的功能，调用AddCapabilities将需要的功能启用
void enableCapabilities() {
	jvmtiCapabilities *capabilities_ptr = malloc(sizeof(jvmtiCapabilities));
	jvmtiError error = (*jvmti_env)->GetCapabilities(jvmti_env,
            capabilities_ptr);
	checkResult("GetCapabilities result", error);
	if(capabilities_ptr != NULL) {
		_LOG_(LOG_INFO,"can_generate_field_modification_events:%d\n", capabilities_ptr->can_generate_field_modification_events);
	}
	if(! capabilities_ptr->can_generate_field_modification_events ) {
		capabilities_ptr->can_generate_field_modification_events = 1;
		error = (*jvmti_env)->AddCapabilities(jvmti_env,
				capabilities_ptr);
		checkResult("AddCapabilities result", error);
	}
	free(capabilities_ptr);
}


char* gatherJvmInfoCallback(char* param, ssize_t sz) {
	struct timespec *timePtr = malloc(sizeof(struct timespec));
	int ret = clock_gettime(CLOCK_REALTIME, timePtr);
	if(ret == -1) {
		_LOG_(LOG_ERROR, "clock_gettime() error %s\n", strerror(errno));
	}
	timePtr->tv_sec += 60;
	timePtr->tv_nsec=0;
	_LOG_(LOG_DEBUG, "before  sem_timedwait\n");
	ret =  sem_timedwait(&g_sem_t, timePtr);
	free(timePtr);
	
	_LOG_(LOG_DEBUG, "after sem_timedwait\n");
	char *buff =NULL;
	if(ret != -1 && errno !=ETIMEDOUT) {
		//from 共享区
		int sz = g_pos - g_buf;
		buff = malloc(sizeof(char) * sz + 1);
		memset(buff, 0, sz + 1);
		strncpy(buff, g_buf, sz);
		memset(g_buf, 0, sz + 1);
		g_pos = g_buf;
	} else if (errno ==ETIMEDOUT) {
		_LOG_(LOG_INFO, "sem_timedwait wakeup with ETIMEDOUT no data gathered\n");
		buff = "no data";
	} else {
		buff = strerror(errno);
		_LOG_(LOG_ERROR, "sem_init() sem_timedwait %s\n", buff);
	}
	
	return buff;
}

