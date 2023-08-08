/*!
 * \file salf.c
 * \brief SALF
 * \author David Kezlinek <kezlidav@fit.cvut.cz>
 * \date 2023
 */
/*
 * Copyright (C) 2023 CESNET
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
  BASIC("salf", "This module receives data from the input interface and selects it to the output interface based on given active learning strategy. The module does not have any ML capabilities; it makes decisions on FEATURE_PREDICT_PROBA field in UniRec.",1,1)

#define MODULE_PARAMS(PARAM) \
PARAM('b', "budget", "Every strategy is limited by budget. This parameter specifies the budget. This number should be in interval [0,1] and it is interpreted as percentage of the data.", required_argument, "int32") \
PARAM('q', "query-strategy", "Number of the query strategy to be used.  0 - Random Strategy  1 -  Fixed Uncertainty Strategy 2 - Variable Uncertainty Strategy  3 -  Uncertainty Strategy with Randomization", required_argument, "int32") \
PARAM('t', "threshold", "labeling threshold for Fixed uncertainty strategy", required_argument, "double")\
PARAM('s', "step", "adjusting step", required_argument, "double")\
PARAM('d', "deviation", "Standard deviation of the threshold randomization used in Uncertainty Strategy with Randomization", required_argument, "double")\
PARAM('n', "no-eof", "Do not send terminate message vie output IFC.", no_argument, "none")




static char stop = 0; /*!< Global variable used by signal handler to end the traffic repeater. */
static int verb = 0; /*< Global variable used to print verbose messages. */
static char sendeof = 1;

static double budget =0.5;
static double labeling_threshold =0.5;
static double step =0.4;
static double t_deviation=1; 

TRAP_DEFAULT_SIGNAL_HANDLER(stop = 1)
 


char random_strategy(const void *data,ur_template_t * in_tmplt,int fieldID){
   return (get_uniform_random() < budget);
}

char fixed_uncertainty_strategy(const void *data,ur_template_t * in_tmplt,int fieldID){
   double probability = (*(double *)  ((char *)(data) + (in_tmplt)->offset[fieldID]));
   return(probability < labeling_threshold);
}

char variable_uncertainty_strategy(const void *data,ur_template_t * in_tmplt,int fieldID){
   static double threshold =1.0;
   static double u =1.0;
   static long t = 0;
   t++;
   if(t >= T_MAX){
      t=1;
      u /= T_MAX;
   }

   if(u/t < budget){
      double probability = (*(double *)  ((char *)(data) + (in_tmplt)->offset[fieldID]));
      if(probability < threshold){
         u++;
         threshold *= 1-step;
         return 1;
      }else{
         threshold *= step+1;
         return 0;
      }
   }else{
      return 0;
   }
} 


char uncertainty_strategy_with_randomization(const void *data,ur_template_t * in_tmplt,int fieldID){
   static double threshold =1.0;
   static double u =1.0;
   static long t = 0;
   t++;
   if(t >= T_MAX){
      t=1;
      u /= T_MAX;
   }

   if(u/t < budget){
      double probability = (*(double *)  ((char *)(data) + (in_tmplt)->offset[fieldID]));
      if(probability < (threshold * normal_distribution(1,t_deviation))){
         u++;
         threshold *= 1-step;
         return 1;
      }else{
         threshold *= step+1;
         return 0;
      }
   }else{
      return 0;
   }
}

void salf(int query_strategy)
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
   int fieldID =0; //field ID of argmax P 
   char (* strategy_fnc)(const void *, ur_template_t * ,int) = &random_strategy;

   switch (query_strategy){
   case 0:
      strategy_fnc = &random_strategy;
      break;
   case 1:
      strategy_fnc = &fixed_uncertainty_strategy;
      break;
   case 2:
      strategy_fnc = &variable_uncertainty_strategy;
      break;
   case 3:
      strategy_fnc = &uncertainty_strategy_with_randomization;
      break;      
   default:
      break;
   }

   data_size = 0;
   data = NULL;
   if (verb) {
      fprintf(stderr, "Info: Initializing salf...\n");
   }
   clock_gettime(CLOCK_MONOTONIC, &start);

   //set NULL to required format on input interface

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
                  fprintf(stderr, "Info: Final record received, salf...\n");
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
            
            if(ur_define_set_of_fields(spec) !=UR_OK){
               if (verb) {
                  fprintf(stderr, "Error: Unirec fields could not be defined...\n");
               }
               return;
            }
            in_tmplt = ur_create_template_from_ifc_spec(spec);
            fieldID =ur_get_id_by_name("FEATURE_OUTPUT_PROBA");
            if (fieldID < 0 || in_tmplt == NULL )
            {
               if (verb) {
                  fprintf(stderr, "Error: template...\n");
               }
               return;
            }
            if(!ur_is_present(in_tmplt,fieldID)){
               if (verb) {
                  fprintf(stderr, "Error: field is not present in template...\n");
               }
               ur_free_template(in_tmplt);
               return;               
            }
            // Set the same data format to repeaters output interface
            trap_set_data_fmt(0, TRAP_FMT_UNIREC, spec);
         }
         
         if (stop == 1 && sendeof == 0){
            /* terminating module without eof message */
            break;
         } else {
            
            if(!(*strategy_fnc)(data,in_tmplt,fieldID)){
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
   fprintf(stderr, "Info: %% of Flows sent:%16.2f%%"  "\n", cnt_r > 0 ?  ((double)cnt_s/ (double)cnt_r)*100: 0);
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
   int query_strategy =0;
   
   srand(time(NULL)); // randomize seed
   
   while ((opt = TRAP_GETOPT(argc, argv, module_getopt_string, long_options)) != -1) {
      switch (opt) {
      case 'n':
         sendeof = 0;
         break;
      case 'b'://budget
         budget = strtod(optarg, NULL);
         
         break;
      case 'q'://query strategy
         query_strategy = atoi(optarg);
         break;
      case 't'://treshold
         labeling_threshold = strtod(optarg, NULL);
         break;
      case 's'://step
         step = strtod(optarg, NULL);
         break;
      case 'd'://deviation
         t_deviation = strtod(optarg, NULL);
         break;
      }
   }

   salf(query_strategy);

   TRAP_DEFAULT_FINALIZATION();
   FREE_MODULE_INFO_STRUCT(MODULE_BASIC_INFO, MODULE_PARAMS)

   return EXIT_SUCCESS;
}