// 헤더 영역은 24 바이트 차지
struct _smheader {	
	size_t size ; // 헤더 다음에 오는 데이터 영역의 길이
	uint8_t used ; // 사용 여부를 나타내는 단일 바이트 속성
	struct _smheader * next ; // 다음 데이터 헤더를 가리키는 포인터
};

typedef struct _smheader 	smheader ;
typedef struct _smheader *	smheader_ptr ;

// smalloc_mode에 사용됨
typedef 
	enum {
		bestfit, worstfit, firstfit
	} 
	smmode ;

void * smalloc (size_t s) ;
void * smalloc_mode (size_t s, smmode m) ;

void sfree (void * p) ;

void * srealloc (void * p, size_t s) ;

void smcoalesce () ;

void smdump () ;
