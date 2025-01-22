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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <stdio.h>

enum {
  TK_NOTYPE = 256, TK_EQ, TK_DEC 

  /* TODO: Add more token types */
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  
  [3]={"[1-9][0-9]?", TK_DEC},  //十进制数
  [4]={"\\-", '-'},      //减法
  [5]={"\\*", '*'},      //乘法
  [6]={"/", '/'},        //除法
  [7]={"\\(", '('},
  [8]={"\\)", ')'} 
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

	//如果造成缓冲区溢出问题
	if (pmatch.rm_eo >= 32){
		panic("The length of the regular expression is too long.");
	}

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
        	case TK_NOTYPE: break; //遇到空格，直接跳过
                default:
			tokens[nr_token].type = rules[i].token_type;
			strncpy(tokens[nr_token].str, substr_start, substr_len);
			tokens[nr_token].str[substr_len] = 0;
			nr_token++;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

//判断表达式中所有的括号是否匹配,即判断表达式是否合法
static bool _check_parentheses(int p, int q){
	if (p == q){ //表达式为'()',不合法
		return false;
	}
	else{ //不会出现p>q的情况了
		int left_num = 0; //左括号的数量
		for (int i = p; i <= q; i++){
			if (left_num == 0 && tokens[i].str[0] == ')'){
				return false;
			}
			else if (tokens[i].str[0] == '('){
				left_num++;
			}
			else if (tokens[i].str[0] == ')'){
				left_num--;
			}
		}
		if (left_num == 0){
			return true;
		}	
		else{
			return false;
		}
	}
}

static bool check_parentheses(int p, int q){
	if (tokens[p].str[0] == '(' && tokens[q].str[0] == ')'){
		return _check_parentheses(p+1, q-1);
	}
	else{ //两端不是括号，一定不合法
		return false;
	}
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
//  TODO();

  check_parentheses(0, nr_token-1);
  return 0;
}
