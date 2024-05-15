#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "smalloc.h"

smheader_ptr smlist = 0x0 ;
#include <unistd.h>
void *smalloc(size_t s) {
    // 데이터 헤더의 포인터
    smheader_ptr current = smlist;
    smheader_ptr prev = NULL;
    // 페이지 크기 가져오기
    size_t page_size = getpagesize();
    // 사용되지 않은 데이터 영역 중에 크기가 s+24 이상인 영역이 있는지 검사
    while (current != NULL) {
        if (current->used == 0 && current->size >= s + 24) {
            break;
        }
        prev = current;
        current = current->next;
    }
    // 만약 해당 크기의 빈 공간을 찾지 못한 경우
    if (current == NULL) {
        // 페이지 추가
        void *page = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (page == MAP_FAILED) {
            perror("mmap");
            return NULL;
        }
        // 새로운 데이터 헤더 생성
        smheader_ptr new_header = (smheader_ptr)page;
        new_header->size = s;
        new_header->used = 1;
        new_header->next = NULL;
        // 연결 리스트에 새로운 데이터 헤더 추가
        if (prev == NULL) {
            smlist = new_header;
        } else {
            prev->next = new_header;
        }
        // 데이터 영역의 첫 번째 주소 반환
        return (void *)(new_header + 1);
    }
    
    // 데이터 영역 분할
    if (current->size > s + 24) {
        // 새로운 데이터 헤더 생성
        smheader_ptr new_header = (smheader_ptr)((void *)current + s + 24);
        new_header->size = current->size - s - 24;
        new_header->used = 0;
        new_header->next = current->next;
        // 현재 헤더 업데이트
        current->size = s;
        current->used = 1;
        current->next = new_header;
    }
    // 데이터 영역의 첫 번째 주소 반환
    return (void *)(current + 1);
}
void *smalloc_mode(size_t s, smmode m) {
    smheader_ptr heap_start = NULL;
    switch (m) {
        case 0: // first fit
            // heap_start가 NULL인 경우 첫 번째 메모리 블록을 할당하고 반환
            if (heap_start == NULL) {
                heap_start = (smheader_ptr)malloc(s + sizeof(smheader));
                heap_start->size = s;
                heap_start->used = 1;
                heap_start->next = NULL;
                return (void *)(heap_start + 1);
            }
            smheader_ptr current = heap_start; // 최초적합 블록을 저장할 포인터 초기화
            smheader_ptr prev = NULL; // 리스트를 순회할 현재 블록 포인터
            // 리스트를 순회해 처음 발견된 적절한 메모리 블록을 할당하고 반환
            while (current != NULL) {
                // 사용되지 않은 블록이면서 요청한 크기보다 크거나 같은 블록을 찾음
                if (!current->used && current->size >= s) {
                    // 현재 블록을 요청한 크기로 사용하고, 필요한 경우 분할
                    if (current->size >= s + sizeof(smheader) + sizeof(uint8_t)) {
                        smheader_ptr new_block = (smheader_ptr)((char *)current + s + sizeof(smheader));
                        new_block->size = current->size - s - sizeof(smheader);
                        new_block->used = 0;
                        new_block->next = current->next;
                        current->next = new_block;
                        current->size = s;
                    }
                    current->used = 1; // current가 가리키는 메모리블록을 사용 중으로 표시해 다시 할당되지 않도록 하고 사용 중인 메모리 블록을 추적 가능하게 함
                    return (void *)(current + 1); // 데이터 영역의 시작 위치 반환
                }
                prev = current;
                current = current->next; // 다음 블록으로 이동
            }
            // 적합한 블록을 찾지 못한 경우, 새로운 메모리 블록을 리스트에 추가하여 할당
            smheader_ptr new_block = (smheader_ptr)malloc(s + sizeof(smheader));
            new_block->size = s;
            new_block->used = 1;
            new_block->next = NULL;
            prev->next = new_block;
            return (void *)(new_block + 1); // 데이터 영역의 시작 위치 반환
        case 1:
            // best fit
            {
                smheader_ptr best_fit = NULL; // 최적적합 블록을 저장할 포인터 초기화
                smheader_ptr current = heap_start; // 리스트를 순회할 현재 블록 포인터
                // 리스트를 순회해 적합한 블록 찾음
                while (current != NULL) {
                    // 사용되지 않고 요청한 크기보다 크거나 같은 블록 찾음
                    if (!current->used && current->size >= s) {
                        // 현재 best fit이 없거나 현재 블록이 더 작으면 best fit 갱신
                        if (best_fit == NULL || current->size < best_fit->size) {
                            best_fit = current;
                        }
                    }
                    current = current->next; // 다음 노드로 이동
                }
                // 적절한 블록을 찾으면 사용하고 필요하면 분할해 사용
                if (best_fit != NULL) {
                    if (best_fit->size >= s + sizeof(smheader) + sizeof(uint8_t)) {
                        smheader_ptr new_block = (smheader_ptr)((char *)best_fit + s + sizeof(smheader));
                        new_block->size = best_fit->size - s - sizeof(smheader);
                        new_block->used = 0;
                        new_block->next = best_fit->next;
                        best_fit->next = new_block;
                        best_fit->size = s;
                    }
                    best_fit->used = 1;
                    return (void *)(best_fit + 1); // 데이터 영역 시작 위치 반환
                }
                // 적절한 블록을 찾지 못한 경우 새로운 메모리 블록 할당
                smheader_ptr new_block = (smheader_ptr)malloc(s + sizeof(smheader));
                new_block->size = s;
                new_block->used = 1;
                new_block->next = NULL;
                // heap_start가 비어있거나 새로운 블록의 크기가 heap_start보다 작은 경우
                if (heap_start == NULL || new_block->size < heap_start->size) {
                    new_block->next = heap_start; // 새로운 블록을 리스트의 맨 앞에 삽입
                    heap_start = new_block;
                } else {
                    // 리스트를 순회하면서 올바른 위치에 블록을 삽입
                    smheader_ptr prev = NULL;
                    current = heap_start;
                    while (current != NULL && current->size <= new_block->size) {
                        prev = current;
                        current = current->next;
                    }
                    prev->next = new_block;
                    new_block->next = current;
                }
                return (void *)(new_block + 1); // 데이터 영역의 시작 위치 반환
            }
        case 2:
            // worst fit
            {
                smheader_ptr worst_fit = NULL; // 최악적합 블록을 저장할 포인터 초기화
                smheader_ptr current = heap_start; // 리스트를 순회할 현재 블록 포인터
                // 리스트를 순회하면서 최악적합 블록을 찾음
                while (current != NULL) {
                    // 사용되지 않은 블록이고 요청한 크기보다 크거나 같은 블록을 찾음
                    if (!current->used && current->size >= s) {
                        // 최악적합 블록 갱신
                        if (worst_fit == NULL || current->size > worst_fit->size) {
                            worst_fit = current;
                        }
                    }
                    current = current->next; // 다음 블록으로 이동
                }
                // 최악적합 블록을 찾은 경우
                if (worst_fit != NULL) {
                    // 최악적합 블록을 요청한 크기로 사용하고 필요한 경우 분할
                    if (worst_fit->size >= s + sizeof(smheader) + sizeof(uint8_t)) {
                        smheader_ptr new_block = (smheader_ptr)((char *)worst_fit + s + sizeof(smheader));
                        new_block->size = worst_fit->size - s - sizeof(smheader);
                        new_block->used = 0;
                        new_block->next = worst_fit->next;
                        worst_fit->next = new_block;
                        worst_fit->size = s;
                    }
                    worst_fit->used = 1;
                    return (void *)(worst_fit + 1); // 데이터 영역의 시작 위치 반환
                }
                // 최악적합 블록을 찾지 못한 경우, 새로운 메모리 블록 할당
                smheader_ptr new_block = (smheader_ptr)malloc(s + sizeof(smheader));
                new_block->size = s;
                new_block->used = 1;
                new_block->next = NULL;
                // 힙이 비어있거나 새로운 블록의 크기가 가장 큰 경우
                if (heap_start == NULL || new_block->size > heap_start->size) {
                    new_block->next = heap_start;
                    heap_start = new_block;
                } else {
                    // 블록을 적절한 위치에 삽입하여 힙을 유지
                    smheader_ptr prev = NULL;
                    current = heap_start;
                    while (current != NULL && current->size >= new_block->size) {
                        prev = current;
                        current = current->next;
                    }
                    prev->next = new_block;
                    new_block->next = current;
                }
                return (void *)(new_block + 1); // 데이터 영역의 시작 위치 반환
            }
        default:
            // 잘못된 모드인 경우 smalloc 호출
            return smalloc(s);
    }
}
void sfree (void * p)
{
   // TODO
    smheader * header = (smheader *)((char *)p - sizeof(smheader)); // 메모리 블록의 헤더를 가져옴
    header->used = 0; // 사용 여부를 변경하여 해제
}

/*
void *srealloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        // ptr이 NULL이면 malloc과 동일
        return smalloc(size);
    }
    // 이전 메모리 블록의 헤더를 가져옴
    smheader *header = (smheader *)((char *)ptr - sizeof(smheader));
    // 이전 메모리 블록의 크기를 가져옴
    size_t old_size = header->size;
    // 새로운 메모리 블록 할당

    void *new_ptr = smalloc(size);
    if (new_ptr == NULL) {
        // 할당 실패 시 NULL 반환
        return NULL;
    }
    // 이전 데이터를 새로운 메모리 블록으로 복사
    size_t i;
    char *src = (char *)ptr;
    char *dest = (char *)new_ptr;
    for (i = 0; i < (size < old_size ? size : old_size); i++) {
        dest[i] = src[i];
    }
    // 이전 메모리 블록 해제
    sfree(ptr);
    return new_ptr;
}
*/

void *srealloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        // ptr이 NULL이면 malloc과 동일
        return smalloc(size);
    }
    // 이전 메모리 블록의 헤더를 가져옴
    smheader *header = (smheader *)((char *)ptr - sizeof(smheader));
    // 이전 메모리 블록의 크기를 가져옴
    size_t old_size = header->size;

    // 새로운 size가 이전 size보다 작거나 같은 경우에는 기존 메모리 공간을 재사용
    if (size <= old_size) {
        header->size = size; // size 값을 변경
        return ptr;
    }

    // 새로운 size에 해당하는 메모리 공간을 할당
    void *new_ptr = smalloc(size);
    if (new_ptr == NULL) {
        // 할당 실패 시 NULL 반환
        return NULL;
    }

    // 새로운 메모리 블록의 다음 포인터 계산
    char *new_end = (char *)new_ptr + size;
    // 기존 메모리 블록의 다음 포인터 계산
    char *old_end = (char *)ptr + old_size;

    // 다음 노드를 침범하는지 확인
    if (header->next != NULL && new_end > (char *)header->next) {
        // 다음 노드를 침범하는 경우
        smheader *next_node = header->next;
        while (next_node != NULL) {
            // s+24 바이트 이상의 공간을 찾음
            if ((char *)next_node - (char *)header >= size + sizeof(smheader)) {
                // 새로운 노드 생성
                smheader *new_node = (smheader *)((char *)header + old_size + sizeof(smheader));
                new_node->size = (char *)next_node - (char *)new_node - sizeof(smheader);
                new_node->used = 0;
                new_node->next = next_node;
                header->next = new_node;
                // 이전 데이터를 새로운 메모리 공간으로 복사
                memcpy(new_ptr, ptr, old_size);
                // 이전 메모리 블록 해제
                sfree(ptr);
                return new_ptr;
            }
            next_node = next_node->next;
        }
        // 새로운 노드를 찾지 못한 경우
        sfree(new_ptr);
        return NULL;
    }

    // 이전 데이터를 새로운 메모리 공간으로 복사
    memcpy(new_ptr, ptr, old_size);

    // 이전 메모리 블록 해제
    sfree(ptr);

    return new_ptr;
}



void smcoalesce() {
    smheader *curr = smlist;
    smheader *prev = NULL;
    // 순회하면서 미사용 데이터 영역을 찾아 병합
    while (curr != NULL && curr->next != NULL) {
        if (!(curr->used) && !(curr->next->used)) {
            // 현재 데이터 영역과 다음 데이터 영역이 모두 미사용인 경우 병합
            curr->size += sizeof(smheader) + curr->next->size;
            curr->next = curr->next->next;
        } else {
            // 다음 데이터 영역으로 이동
            prev = curr;
            curr = curr->next;
        }
    }
}
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