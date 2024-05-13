#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "smalloc.h" 

// 첫번째 데이터 노드를 가리키는 정적 변수
// 링크드 리스트의 시작점이며, 데이터 헤더를 따라 이동한다
smheader_ptr smlist = 0x0 ; 

/*
• smalloc (size_t s)

크기가 s 이상인 사용되지 않은 데이터 영역을 선택합니다.
-- 그러한 영역이 없다면, 더 많은 페이지를 추가하기 위해 mmap()을 사용합니다.선택한 데이터 영역의 크기가 s + 24보다 크다면, 두 개로 분할합니다.
-- 선택한 데이터 영역의 크기를 s로 업데이트합니다.
-- 남은 데이터 영역에 대한 새 헤더를 추가합니다.데이터 영역의 첫 번째 주소를 반환합니다.
*/
// 요청된 디바이스를 제공하는 데이터 영역의 시작 주소 반환
// 구현 방법 
// 1. 링크드 리스트로 사용되지 않은 데이터 영역 찾기
//    이 영역의 크기는 사용자 프로그램이 요청한 바이트보다 크거나 같아야 함
//    그러나 링크드 리스트에 이와 같은 데이터 영역이 없다면 이 호출을 취소하고, 페이지 추가해야 함
// 2. 선택한 데이터 영역의 크기가 s+ 24보다 크다면 이를 둘로 나눔
// 3. 사용자가 요청한 크기에 맞게 데이터 영역의 첫 번째 주소를 반환
void *smalloc(size_t s) {
    // 페이지 내에서 사용 가능한 영역을 찾기 위한 변수
    smheader_ptr curr = smlist;
    smheader_ptr prev = NULL;

    // 페이지 내에서 사용 가능한 영역을 찾는 과정
    while (curr != NULL) {
        if (!curr->used && curr->size >= s + sizeof(smheader)) {
            // 해당 영역을 찾았을 경우
            if (curr->size >= s + sizeof(smheader) + 4096) {
                // 해당 영역이 필요한 크기보다 크다면 나머지를 새로운 블록으로 분할
                smheader_ptr new_block = (smheader_ptr)((char *)curr + s + sizeof(smheader));
                new_block->size = curr->size - s - sizeof(smheader);
                new_block->used = 0;
                new_block->next = curr->next;
                curr->size = s + sizeof(smheader);
                curr->next = new_block;
            }
            curr->used = 1;
            return (void *)(curr + 1); // 데이터 영역의 시작 포인터 반환
        }
        prev = curr;
        curr = curr->next;
    }

    // 사용 가능한 영역을 찾지 못한 경우 새로운 페이지 할당
    size_t page_size = s + sizeof(smheader) + 4096; // 필요한 페이지 크기 계산
    smheader_ptr new_page = (smheader_ptr)mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_page == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    // 새로운 페이지에 대한 링크드 리스트 생성
    new_page->size = page_size - sizeof(smheader);
    new_page->used = 1;
    new_page->next = NULL;

    // 리스트 연결
    if (prev == NULL) {
        smlist = new_page;
    } else {
        prev->next = new_page;
    }

    return (void *)(new_page + 1); // 데이터 영역의 시작 포인터 반환
}


/*
• smalloc_mode (size_t s, smmode m)

지정된 모드 m과 일치하는 사용되지 않은 데이터 영역을 선택합니다.
-- 모드: bestfit, worstfit, firstfit다른 동작은 smalloc (size_t s)와 동일합니다.
*/
// smalloc()에서 사용되지 않은 데이터 영역을 선택할 때 여러 후보 중 크기가 s와 같거나 큰 후보가 있다면
// 어떤 것을 선택해야 하는지(bestfit, worstfit, firstfit)
// smalloc.h에 관련 type 존재
// 사용자가 smalloc을 호출할 때 전략 제어 가능
// 사용자가 지정한 모드에 따라 smalloc 함수가 사용할 데이터 영역 선택
void * smalloc_mode (size_t s, smmode m){
	
}

void sfree (void * p) 
{
	// TODO 
}

void * srealloc (void * p, size_t s) 
{
	// TODO
	return 0x0 ; // erase this 
}

void smcoalesce ()
{
	//TODO
}

// 데이터 영역에 대한 정보 읽음
void smdump () 
{
	smheader_ptr itr ;

	printf("==================== used memory slots ====================\n") ;
	int i = 0 ;
	for (itr = smlist ; itr != 0x0 ; itr = itr->next) {
		if (itr->used == 0)
			continue ;

		printf("%3d:%p:%8d:", i, ((void *) itr) + sizeof(smheader), (int) itr->size) ;

		int j ;
		char * s = ((char *) itr) + sizeof(smheader) ;
		for (j = 0 ; j < (itr->size >= 8 ? 8 : itr->size) ; j++)  {
			printf("%02x ", s[j]) ;
		}
		printf("\n") ;
		i++ ;
	}
	printf("\n") ;

	printf("==================== unused memory slots ====================\n") ;
	i = 0 ;
	for (itr = smlist ; itr != 0x0 ; itr = itr->next, i++) {
		if (itr->used == 1)
			continue ;

		printf("%3d:%p:%8d:", i, ((void *) itr) + sizeof(smheader), (int) itr->size) ;

		int j ;
		char * s = ((char *) itr) + sizeof(smheader) ;
		for (j = 0 ; j < (itr->size >= 8 ? 8 : itr->size) ; j++) {
			printf("%02x ", s[j]) ;
		}
		printf("\n") ;
		i++ ;
	}
	printf("\n") ;
}
