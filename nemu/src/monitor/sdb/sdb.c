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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"
#include <../include/memory/paddr.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_si(char *args){
  //如果没有给出N，args为NULL,执行单步执行，否则根据给出的N来执行
  int step = 1;
  if (args){
    char *endptr;
    step = strtoul(args, &endptr, 10);
    if (*endptr != '\0'){
      printf("Please input an integer. Correct form: si N\n");
      return 0;
    }
  }

  //根据step来执行
  cpu_exec(step);
  printf("Step Execute = %d\n", step);

  return 0; 
} 

static int cmd_info(char *args){
  //打印寄存器状态
  if (*args == 'r'){
    isa_reg_display();
  }
  else if (*args == 'w'){
    wp_show();
  }
  else{
    printf("Please input: info w or info r.\n");
  }

  return 0;
}

static int cmd_x(char *args){
  //将args进一步分别成数字和表达式
  char *num_tmp = strtok(args, " ");
 //剩下的args就是表达式
 
  char *endptr;
  int num = strtol(num_tmp, &endptr, 10);
  if (*endptr != '\0'){
    printf("Please input an integer. Correct form: x N expr.\n");
    return 0;
  }
  
  //expr暂时只能是十六进制数
  char *addr_tmp = num_tmp + strlen(num_tmp) + 1; 

  //args是表达式
  bool success;
  paddr_t expr_to_addr = expr(addr_tmp, &success);
  if (success == false){
    printf("Invalid expression. Correct form: x N expr.\n");
    return 0;
  }  

  for (int i = 0; i < num; i++){
    printf("%#x: %#x\n", expr_to_addr, paddr_read(expr_to_addr, 4));
    expr_to_addr += 4;
  }

  return 0;
}

static int cmd_p(char *args){
  bool success;
  uint32_t value = expr(args, &success);

  if (success == false){
	  printf("Cannot not make token\n");
	  return 0;
  }
  else{
          printf("The value of the expression is: %u(dec), 0x%x\n", value, value);
  }
  return 0;  
}

static int cmd_w(char *args){
	if (!args){
		printf("Correct form: w expr.\n");
		return 0;
	}

	bool success;
	word_t ret = expr(args, &success);
	if (success == false){
		printf("w expr: expression is invalid.\n");
	}
	else{
		WP* wp = new_wp();
		strncpy(wp->expr, args, 65536);
		wp->old_value = ret;
		printf("Set watchpoint %d: %s.\n", wp->NO, wp->expr);
	}
	return 0;
}

static int cmd_d(char *args){
	if (!args){
		printf("Correct form: d N.\n");
		return 0;
	}

	char *num = strtok(NULL, " ");
	int no = atoi(num);
	free_wp(no);
	printf("Watchpoint number %d has been deleted.\n", no);

	return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  [0]={ "help", "Display information about all supported commands", cmd_help },
  [1]={ "c", "Continue the execution of the program", cmd_c },
  [2]={ "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */
  [3]={"si", "Single-step execution", cmd_si},
  [4]={"info", "Print out program state", cmd_info},
  [5]={"x", "scan memory", cmd_x},
  [6]={"p", "calculate the value of the expression", cmd_p},
  [7]={"w", "set a watchpoint", cmd_w},
  [8]={"d", "delete a watchpoint", cmd_d},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void test_expr(){
  FILE *fp = fopen("/home/sherry/Desktop/ysyx-workbench/nemu/tools/gen-expr/input", "r");
  if (fp == NULL) perror("test_expr error");

  char *e = NULL;
  uint32_t correct_res;
  size_t len = 0;
  ssize_t read;
  bool success = false;

  while (true) {
    if(fscanf(fp, "%u ", &correct_res) == -1) break;
    read = getline(&e, &len, fp);
    e[read-1] = '\0';

    word_t res = expr(e, &success);

    assert(success);
    if (res != correct_res) {
      puts(e);
      printf("expected: %u, got: %u\n", correct_res, res);
      assert(0);
    }
  }

  fclose(fp);
  if (e) free(e);

  Log("expr test pass");
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  //测试表达式求值
  test_expr();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
