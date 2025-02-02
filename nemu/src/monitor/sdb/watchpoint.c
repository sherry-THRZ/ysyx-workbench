/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include "sdb.h"

#define NR_WP 32

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    
    wp_pool[i].expr[0] = 0;
    wp_pool[i].old_value = 0;    
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */
WP* new_wp(){
	if (free_ == NULL){
		printf("There's no free watchpoint available.\n");
		assert(0);
	}

	WP* wp = free_;
	free_ = free_->next;
	wp->next = head;
	head = wp;

	return wp;
}

//改称删除指定序号来删除
void free_wp(int no){
	WP* p = head;
	if (head == NULL){
		printf("There's no watchpoint in use.\n");
		assert(0);
	}	
	
	WP* q = NULL; //记录p的前一个链表
	for (; p && (p->NO != no); q = p, p = p->next);
	if (p == NULL){
		printf("There's no matched watchpoint number: %d\n", no);
		assert(0);
	}
	else if (q == NULL && p == head){ //匹配的是链表第一个元素
		head = p->next;  
	}
	else{
		q->next = p->next; //在head链表中删除p指向的元素
	}	
	p->next = free_;
	free_ = p;

	return;
}

void wp_show(){      
  WP* wp = head;
  if (wp == NULL){
    printf("There's no watchpoint.\n");
    return;
  }
  printf("%-10s%-10s%-10s\n", "Num", "What", "Value");
  for (; wp; wp = wp->next){
    printf("%-10d%-10s0x%-10x\n", wp->NO, wp->expr, wp->old_value);
  }
  return;
}

//检查监视点的值是否改变
void check_watchpoint(){
	WP* wp = head;
	bool success = false;
	word_t new_value;

	for (; wp != NULL; wp = wp->next){
		new_value = expr(wp->expr, &success);
		if (wp->old_value != new_value){
			printf("Value of watchpoint %d: %s has changed.\nold_value = %u\nnew_value = %u\n", wp->NO, wp->expr, wp->old_value, new_value);
			wp->old_value = new_value;
			nemu_state.state = NEMU_STOP;
			return; //找到一个变化的就return
		}
	}

	return;
}
