/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

// 选择一个0到n-1之间的随机数
static int choose(int n) {
    return rand() % n;
}

// 生成一个随机数字并将其追加到buf中
static void gen_num() {
    int num = rand() % 100 + 1; // 生成1到100之间的随机数
    char term[32];
    snprintf(term, sizeof(term), "%d", num);
    strcat(buf, term);
}

// 将字符ch追加到buf中
static void gen(char ch) {
    char term[2];
    term[0] = ch;
    term[1] = '\0';
    strcat(buf, term);
}

// 生成一个随机运算符并将其追加到buf中
static void gen_rand_op() {
    char op;
    switch (choose(4)) {
        case 0:
            op = '+';
            break;
        case 1:
            op = '-';
            break;
        case 2:
            op = '*';
            break;
        case 3:
            op = '/';
            break;
    }
    gen(op);
}

void gen_rand_expr() {
    switch (choose(3)) {
        case 0:
            gen_num();
            break;
        case 1:
            gen('(');
            gen_rand_expr();
            gen(')');
            break;
        default:
            gen_rand_expr();
            gen_rand_op();
            gen_rand_expr();
            break;
    }
}
int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    buf[0] = '\0';
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);
    printf("%u %s\n", result, buf);
  }
  return 0;
}

