/* Standalone protocol diagnostic; stdin is framed USB data, stdout is bounded
 * decoded metadata. Uses exactly the production parser/pool budgets. */
#include "bot_frame.h"
#include <stdio.h>
static bot_frame_parser_t parser;static bj_node_t nodes[640];static char strings[8193];static bot_msg_t msg;
int main(void){bot_frame_init(&parser,nodes,640,strings,sizeof(strings));int c,good=0,bad=0;while((c=getchar())!=EOF){bot_frame_result_t r=bot_frame_feed(&parser,(unsigned char)c,&msg);if(r==BOT_FRAME_MSG){good++;printf("decoded=%s nodes=%u",bot_msg_type_name(msg.type),(unsigned)parser.doc.node_used);if(msg.type==BOT_MSG_USAGE){bot_usage_t *u=&msg.body.usage;printf(" usage_rev=%u data_rev=%u subject=%u period=%u page=%u quotas=%u models=%u history=%u",u->view.usage_rev,u->data_rev,u->view.subject,u->view.period,u->view.page,u->quota_count,u->model_count,u->history_count);}puts("");}else if(r==BOT_FRAME_BAD_LINE||r==BOT_FRAME_OVERSIZE){bad++;printf("rejected=%u json_error=%s\n",r,bj_err_name(parser.last_json_err));}}printf("frames=%d rejected=%d\n",good,bad);return good&&!bad?0:1;}
