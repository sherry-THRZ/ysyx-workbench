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
  TK_NOTYPE = 256, TK_EQ, TK_DEC, TK_HEX, TK_REG 

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
  
  [3]={"\\-", '-'},      //减法
  [4]={"\\*", '*'},      //乘法
  [5]={"/", '/'},        //除法
  [6]={"\\(", '('},
  [7]={"\\)", ')'}, 
  [8]={"0[xX][0-9A-Fa-f]+", TK_HEX},
  [9]={"[0-9]+u?", TK_DEC},  //十进制数
  [10]={"\\$[$0-9a-z]{2,3}", TK_REG},
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

static Token tokens[65536] __attribute__((used)) = {};
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
		case TK_DEC:
			   tokens[nr_token].type = rules[i].token_type;
			   strncpy(tokens[nr_token].str, substr_start, substr_len); 
			   if (tokens[nr_token].str[substr_len-1] == 'u'){ //如果使用字面量后缀
				   tokens[nr_token].str[substr_len-1] = 0;
			   }
			   nr_token++;
			   break;
		case TK_HEX:
			   tokens[nr_token].type = rules[i].token_type;
			   strncpy(tokens[nr_token].str, substr_start+2, substr_len-2); //0x不读入
			   nr_token++;
			   break;
		case TK_REG:
			   tokens[nr_token].type = rules[i].token_type;
			   strncpy(tokens[nr_token].str, substr_start+1, substr_len-1); //开头$不读入
			   nr_token++;
			   break;
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
	if (p == q){ //check_parentheses输入的形式是() 中一个数
		if (tokens[p].type == TK_DEC){
			return true;
		}	
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

//判断优先级的函数
//返回值越大代表优先级越高
int priority(int x){
	if ((tokens[x].type == '+') || (tokens[x].type == '-')){
		return 0;
	}	
	else if ((tokens[x].type == '*') || (tokens[x].type == '/')){
		return 1;
	}
	else{
		return 2;
	}
}

//判断主运算符函数
int find_mainop(int p, int q){
	int mainop = -1;
	int lowest_priority = 2;
	int in_colon = 0; //运算符是否在括号李
	for (int i = p; i <= q; i++){
	if (tokens[i].type == '('){
			in_colon++;
		}
		else if (tokens[i].type == ')'){
			in_colon--;
		}
		if (!in_colon){
			if (priority(i) <= lowest_priority){
				lowest_priority = priority(i);
  				mainop = i;
			}
		}
	}
	
	return mainop;
}

uint32_t eval(int p, int q) {
  if (p > q) {
    /* Bad expression */
    printf("Bad Expression: p > q\n");
    //这里如何处理需要修改！
    return -1;
  }
  else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    uint32_t num = 0;

    //最后剩的需要根据类型返回不同的值
    if (tokens[p].type == TK_HEX){
	num = strtoul(tokens[p].str, NULL, 16);
    }
    else if (tokens[p].type == TK_DEC){
        num = strtoul(tokens[p].str, NULL, 10);
    }
    else if (tokens[p].type == TK_REG){
	bool success;
        uint32_t tmp = isa_reg_str2val(tokens[p].str, &success);

	if (success == true){
	  num = tmp;
	}
	else{
	  printf("Do not match the name of any reg\n");
	  return -1;
	}
    }

     return num;
  }
  else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1);
  }
  else {
    //检查是否会出现不合法的表达式
    if (_check_parentheses(p, q) == false){
	    printf("Bad expression, the expression is illegal, it has unmatched parentheses or the thing in the parenthesis is wrong!\n");
	    return -1; //可能需要修改！
    }
    int op = find_mainop(p, q); //主运算符的下标
    
    if (priority(op) == 2){
	    printf("Find the wrong operand: tokens[%d].str =  %s\n", op, tokens[op].str);
	    return -1;
    }

    uint32_t val1 = eval(p, op - 1);
    uint32_t val2 = eval(op + 1, q);

    char op_type = tokens[op].type;
    switch (op_type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': return val1 / val2;
      default: assert(0);
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  *success = true;
  return eval(0, nr_token-1);
}
