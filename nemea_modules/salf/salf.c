/*!
 * \file traffic_repeater.c
 * \author Jan Neuzil <neuzija1@fit.cvut.cz>
 * \author Tomas Cejka <cejkat@cesnet.cz>
 * \date 2013
 * \date 2014
 * \date 2017
 */
/*
 * Copyright (C) 2013-2017 CESNET
 *
 * LICENSE TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#include "salf.h"

trap_module_info_t *module_info = NULL;

#define MODULE_BASIC_INFO(BASIC) \
  BASIC("traffic_repeater","This module receive data from input interface and resend it to the output interface based on given arguments in -i option.",1,1)

#define MODULE_PARAMS(PARAM) \
PARAM('b', "budget", " every strategy is limited by budget. This parameter specifies the budget. This number should be in interval [0,1] and it is interpreted as percentage of the data.", required_argument, "int32") \
PARAM('s', "query-strategy", "Number of the query strategy to be used. ", required_argument, "int32") \
PARAM('n', "no-eof", "Do not send terminate message vie output IFC.", no_argument, "none")

static char stop = 0; /*!< Global variable used by signal handler to end the traffic repeater. */
static int verb = 0; /*< Global variable used to print verbose messages. */
static char sendeof = 1;

static double budget =0.5;

TRAP_DEFAULT_SIGNAL_HANDLER(stop = 1)


char random_strategy(const void *data,ur_template_t * in_tmplt){
   return (get_random() < budget);
}

char fixed_uncertainty_strategy(const void *data,ur_template_t * in_tmplt){
   static int mem =0;


}

char variable_uncertainty_strategy(const void *data,ur_template_t * in_tmplt){


}


void salf(void)
{
   int ret;
   uint16_t data_size;
   uint64_t cnt_r = 0; //Flows received
   uint64_t cnt_s = 0; //Flows sent
   uint64_t cnt_t = 0; //timeouts
   uint64_t diff;
   const void *data;
   struct timespec start, end;
   ur_template_t * in_tmplt= NULL;

   char (* strategy_fnc)(const void *, ur_template_t * ) = &random_strategy;


   switch (diff){
   case 1:
      strategy_fnc =&random_strategy;
      break;
   
   case 2:
      strategy_fnc =&random_strategy;
      break;
   
   default:
      break;
   }

   data_size = 0;
   data = NULL;
   if (verb) {
      fprintf(stderr, "Info: Initializing traffic repeater...\n");
   }
   clock_gettime(CLOCK_MONOTONIC, &start);

   //set NULL to required format on input interface

 //"ipaddr SRC_IP,ipaddr DST_IP,uint16 SRC_PORT,uint16 DST_PORT,uint8 PROTOCOL,uint64 BYTES,uint64 BYTES_REV,uint32 PACKETS,uint32 PACKETS_REV,time TIME_FIRST,time TIME_LAST,string TLS_SNI,uint8 PREDICTION,string EXPLANATION,string LABEL";
   
   trap_set_required_fmt(0, TRAP_FMT_UNIREC, "");

   

   TRAP_REGISTER_DEFAULT_SIGNAL_HANDLER();

   //main loop
   while (stop == 0) {
      ret = trap_recv(0, &data, &data_size);
      if (ret == TRAP_E_OK || ret == TRAP_E_FORMAT_CHANGED) {
         cnt_r++;
         if (ret == TRAP_E_OK && in_tmplt != NULL) {
            if (data_size <= 1) {
               if (verb) {
                  fprintf(stderr, "Info: Final record received, terminating repeater...\n");
               }
               stop = 1;
            }
         } else {
            // Get the data format of senders output interface (the data format of the output interface it is connected to)
            const char *spec = NULL;
            uint8_t data_fmt = TRAP_FMT_UNKNOWN;
            if (trap_get_data_fmt(TRAPIFC_INPUT, 0, &data_fmt, &spec) != TRAP_E_OK) {
               fprintf(stderr, "Data format was not loaded.");
               return;
            }

            if(in_tmplt !=NULL){
               ur_free_template(in_tmplt);
            }
            int test=ur_define_set_of_fields(spec);
            in_tmplt = ur_create_template_from_ifc_spec(spec);

            

            // Set the same data format to repeaters output interface
            trap_set_data_fmt(0, TRAP_FMT_UNIREC, spec);
            
         }
         
         
         if (stop == 1 && sendeof == 0){
            /* terminating module without eof message */
            break;
         } else {

            if(in_tmplt == NULL){
               //TODO co delat
               //continue;
            }

            int ggg =ur_get_id_by_name("PREDICTION");

            uint8_t lol= ur_get(in_tmplt,data,ggg);

            uint8_t kkk = (*(uint8_t *)  ((char *)(data) + (in_tmplt)->offset[ggg]));

            if(!(*strategy_fnc)(data,in_tmplt)){
               continue;
            }

            ret = trap_send(0, data, data_size);
            if (ret == TRAP_E_OK) {
               cnt_s++;
               continue;
            }
            TRAP_DEFAULT_SEND_DATA_ERROR_HANDLING(ret, cnt_t++; continue, break)
         }
      } else {
         TRAP_DEFAULT_GET_DATA_ERROR_HANDLING(ret, cnt_t++; puts("trap_recv timeout"); continue, break)
      }
   }

   clock_gettime(CLOCK_MONOTONIC, &end);
   diff = (end.tv_sec * NS + end.tv_nsec) - (start.tv_sec * NS + start.tv_nsec);
   fprintf(stderr, "Info: Flows received:  %16" PRIu64 "\n", cnt_r > 0 ? cnt_r - 1 : cnt_r);
   fprintf(stderr, "Info: Flows sent:      %16" PRIu64 "\n", cnt_s > 0 ? cnt_s - 1 : cnt_s);
   fprintf(stderr, "Info: Timeouts:        %16" PRIu64 "\n", cnt_t);
   fprintf(stderr, "Info: Time elapsed:    %12" PRIu64 ".%03" PRIu64 "s\n", diff / NS, (diff % NS) / 1000000);

   if(in_tmplt != NULL){
      ur_free_template(in_tmplt);
   }

}




int main(int argc, char **argv)
{
   INIT_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)
   TRAP_DEFAULT_INITIALIZATION(argc, argv, *module_info);
   verb = (trap_get_verbose_level() >= 0);
   signed char opt;

   srand(time(NULL)); // randomize seed

   int query_strategy =0;

   while ((opt = TRAP_GETOPT(argc, argv, module_getopt_string, long_options)) != -1) {
      switch (opt) {
      case 'n':
         sendeof = 0;
         break;
      case 'b':
         char *ptr;
         budget = strtod(optarg, &ptr);
         
         break;
      case 's':
         query_strategy = atoi(optarg);
         break;
      }
   }

   salf();
   TRAP_DEFAULT_FINALIZATION();
   FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)

   return EXIT_SUCCESS;
}