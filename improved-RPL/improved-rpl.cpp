/******************************************************************************
* This file is part of improved-RPL.
*
* improved-RPL is free software: you can redistribute it and/or modify
* it under the terms of the GNU LesserGeneral Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* improved-RPL is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with improved-RPL. If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/


#include "external_interface/external_interface.h"
#include "algorithms/routing/tree/tree_routing.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#define ROOT_NODE 0x0 //0x2028
#define SEND_TO 000200
#define SEND_FROM 6

#define NEIGHBOR_SIZE 256 //256 //65534 is the maximum value. do NOT set this bigger than 255 never!


#define ROUTN_WIDTH 256 //256
#define ROUTN_LEN 32

#define PARENT_TIMEOUT 15

#define OF_TO_USE 0x00
#define MIN_HIGH_TREE 0x00
#define HOP_WEIGHT 0
#define LQI_WEIGHT 1
#define DATA_SIZE 32

#define NEIGHBOR_DIS 0
#define NEIGHBOR_DIS_ACK 1
#define DIO 2
#define DAO 3
#define DAO_RESPONSE 4
#define DAO_ACK 5
#define UP 0
#define DOWN 1
#define DATA 7
#define REPAIR 8
#define DISBAND 9

typedef wiselib::OSMODEL Os;

class rpl
{
public:


    struct metrics
    {
        uint16_t metric1;
        uint16_t metric2;
        uint16_t metric3;
        uint16_t metric4;
    };

    typedef metrics metrics_t;

    struct rpl_dio
    {
        uint16_t root;
        uint16_t parent;
        uint8_t type;
        uint8_t hops;
        uint16_t id;
        uint8_t version;
        uint8_t dest;
        uint8_t given;
        uint16_t rank;
        uint8_t instance_id;
        metrics_t dio_metrics;

    };
    typedef rpl_dio rpl_dio_t;

    struct header
    {
        uint8_t type;
        uint16_t to_node;
        uint16_t from_node;
        char to_routN[ROUTN_LEN];
        char from_routN[ROUTN_LEN];
    };
    typedef header header_t;

    struct rpl_dag
    {
        uint8_t of;

        uint8_t version;
        uint8_t id;
        uint8_t instance_id;
        uint8_t joined;
        uint8_t hops;
        uint16_t rank;
        uint16_t root;
        uint16_t parent;
        char routN[ROUTN_LEN];
        char previous_routN[ROUTN_LEN];
        metrics_t dag_metrics;
    };
    typedef rpl_dag rpl_dag_t;

    struct rpl_dao
    {
        uint8_t instance_id;
        uint8_t dao_sequence;
    };

    typedef rpl_dao rpl_dao_t;

    struct rpl_dao_response
    {
        uint8_t instance_id;
        uint8_t dao_sequence;
    };

    typedef rpl_dao_response rpl_dao_response_t;

    struct rpl_data
    {
        uint8_t instance_id;
        uint8_t direction;
        uint8_t hops;
        //char from[ROUTN_LEN];
        char payload[DATA_SIZE];
    };

    typedef rpl_data rpl_data_t;

//--------------------------------------------------------------------------------------------------

    void init( Os::AppMainParameter& value )
    {
        radio_ = &wiselib::FacetProvider<Os, Os::Radio>::get_facet( value );
        timer_ = &wiselib::FacetProvider<Os, Os::Timer>::get_facet( value );
        debug_ = &wiselib::FacetProvider<Os, Os::Debug>::get_facet( value );


        radio_->reg_recv_callback<rpl,
        &rpl::receive_radio_message>( this );
        // timer_->set_timer<rpl, &rpl::>( 5000, this, 0 );



        timer_->set_timer<rpl, &rpl::discover_neighbors>( 2000, this, 0 );

        rpl_dag_structure.root=0xffff;

        rpl_dag_structure.rank=0xffff;

        rpl_dag_structure.dag_metrics.metric1=0xffff;

        rpl_dag_structure.joined=0xff;

        rpl_dag_structure.of=OF_TO_USE;

        memset(rpl_dag_structure.routN, NULL, ROUTN_LEN);

        memset(rpl_dag_structure.previous_routN, NULL, ROUTN_LEN);

        needs_repair_=0;
        dao_flag=0;
        /// na figei...
        power=1;

        dao_counter=0;
        rpl_dag_structure.joined=0xff;
        for(uint16_t j=0; j<NEIGHBOR_SIZE; j++)
        {
            neighbors[j]=0xffff;
        }


        for (uint8_t i=0; i<=ROUTN_WIDTH-1; i++)
        {
            if(i==ROUTN_WIDTH-1)
                break;

            pending[i][0]=0xffff;
        }


        sequence_counter_=0x00;

        if(radio_->id()==ROOT_NODE)
        {
            rpl_dag_structure.rank=0;
            rpl_dag_structure.root=radio_->id();
            strcpy(rpl_dag_structure.routN, "00");
            rpl_dag_structure.dag_metrics.metric1=0;
            rpl_dag_structure.version=5;
            rpl_dag_structure.joined=0;
            rpl_dag_structure.hops=0;

            debug_->debug("IM ROOT");

        }
        if(radio_->id()==SEND_FROM)
        {
           timer_->set_timer<rpl, &rpl::test_mess>( 60000, this, 0 );
        }
       // if(radio_->id()==4)
       // {
       //     timer_->set_timer<rpl, &rpl::deactivate>( 27000, this, 0 );
       // }
        timer_->set_timer<rpl, &rpl::dio_output>( 2000, this, 0 );
        timer_->set_timer<rpl, &rpl::check_pending>( 4000, this, 0 );

        timer_->set_timer<rpl, &rpl::publish_routn>( 55000, this, 0 );
    }
//--------------------------------------------------------------------------------------------------------------------

  void publish_routn( void* )
    {
        debug_->debug("node %x PUBLISHES routN %s\n", radio_->id(), rpl_dag_structure.routN);
    }
//--------------------------------------------------------------------------------------------------------------------
    void deactivate ( void* )
    {
        power=0;
    }
//--------------------------------------------------------------------------------------------------------------------
    void discover_neighbors( void* )
    {
        if(power==1)
        {
            header_t my_header;
            my_header.type=NEIGHBOR_DIS;
            my_header.from_node=radio_->id();
            radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), ( Os::Radio::block_data_t*)&my_header );
            timer_->set_timer<rpl, &rpl::discover_neighbors>( 10000, this, 0 );
        }
    }

//--------------------------------------------------------------------------------------------------------------------

    void handle_neighbor_discovery (header_t inbox_header)
    {
        //  debug_->debug("node %x received nd from node %x",radio_->id(), inbox_header.from_ipv6[0]);
        if (rpl_dag_structure.joined==0 && inbox_header.from_node==rpl_dag_structure.parent)
        {
            parent_timeout_=0;
            //debug_->debug("node %d reseting parent_timeout\n",radio_->id());
        }
        header_t header;
        header.type=NEIGHBOR_DIS_ACK;

        handle_neighbor_discovery_ack(inbox_header);

        header.to_node=inbox_header.from_node;
        header.from_node=radio_->id();
        radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), ( Os::Radio::block_data_t*)&header );
    }
//--------------------------------------------------------------------------------------------------------------------

    void handle_neighbor_discovery_ack(header_t inbox_header)
    {

        bool used=false;
        uint16_t position=0xffff;
        for (uint16_t i=0; i<NEIGHBOR_SIZE; i++)
        {
            if (position==0xffff && neighbors[i]==0xffff)
            {
                position=i;
            }
            if(neighbors[i]==inbox_header.from_node)
            {
                used=true;
                break;
            }
        }
        if(used==false)
        {
            neighbors[position]=inbox_header.from_node;
            //debug_->debug("node: %x added %x to list\n", radio_->id(), inbox_header.from_node);
        }
    }
//--------------------------------------------------------------------------------------------------------------------
    void dio_output (void *)
    {
        if(power==1)
        {
            if((rpl_dag_structure.joined==0) && (strlen(rpl_dag_structure.routN)<ROUTN_LEN))
            {
                rpl_dio_t dio;

                dio.id = radio_->id();
                dio.dio_metrics = rpl_dag_structure.dag_metrics;
                dio.version = rpl_dag_structure.version;
                dio.instance_id = rpl_dag_structure.instance_id;
                dio.root = rpl_dag_structure.root;
                dio.hops = rpl_dag_structure.hops;

                header_t header;
                header.type=DIO;
                header.from_node=radio_->id();

                strncpy(header.from_routN, rpl_dag_structure.routN, strlen(rpl_dag_structure.routN));
                for (uint16_t i=0; i<=NEIGHBOR_SIZE; i++)
                {
                    if(neighbors[i]==0xffff)
                    {
                        break;
                    }
                    else
                    {
                        header.to_node=neighbors[i];
                    }
                    uint16_t size;
                    size = sizeof(header_t)+sizeof(rpl_dio_t);
                    unsigned char message[size];
                    memcpy(message, &header, sizeof(header_t));
                    memcpy(message+sizeof(header_t),&dio, sizeof(rpl_dio_t));
                    // debug_->debug("metric1: %x, version: %x", dio.dio_metrics.metric1, dio.version);

                    radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);
                }
            }

            timer_->set_timer<rpl, &rpl::dio_output>( 2000, this, 0 );

        }
    }

//--------------------------------------------------------------------------------------------------------------------
    void dio_input (header_t header, rpl_dio_t input_dio, uint8_t LQI)
    {

        if((rpl_dag_structure.joined!=0)) /* auto tha figei */ //&& (strncmp(header.from_routN, rpl_dag_structure.previous_routN, strlen(rpl_dag_structure.previous_routN))!=0))
        {
            //debug_->debug("RESULTTTTTTTTT: %d",strncmp(header.from_routN, rpl_dag_structure.previous_routN, strlen(rpl_dag_structure.previous_routN)));

            if((strlen(rpl_dag_structure.previous_routN)>0) )
            {
                if((strncmp(header.from_routN, rpl_dag_structure.previous_routN, strlen(rpl_dag_structure.previous_routN)-2)==0))
                {
                    //do nothing
                }
                else
                {
                    {

                        uint16_t i;
                        for (i=0; i<=NEIGHBOR_SIZE; i++)
                        {
                            if(i==NEIGHBOR_SIZE || i==0xffff)
                            {
                                break;
                            }

                            if(neighbors[i]==header.from_node)
                            {
                                break;
                            }
                        }

                        if(i!=NEIGHBOR_SIZE)
                        {

                            switch(rpl_dag_structure.of)
                            {
                            case MIN_HIGH_TREE:
                                temp_rank=min_high_tree_OF(input_dio.dio_metrics, LQI);
                                // debug_->debug("node %d temp_rank: %d\n",radio_->id(),temp_rank);
                                break;
                            }

                            if(temp_rank<rpl_dag_structure.rank)
                            {
                                rpl_dag_structure.rank = temp_rank;
                                rpl_dag_structure.hops = input_dio.hops + 1;
                                rpl_dag_structure.parent = input_dio.id;
                                rpl_dag_structure.version = input_dio.version;
                                rpl_dag_structure.dag_metrics.metric1 = input_dio.dio_metrics.metric1+1;
                                rpl_dag_structure.root = input_dio.root;
                                if(radio_->id()!=ROOT_NODE && dao_flag!=1)
                                {
                                    timer_->set_timer<rpl, &rpl::call_dao_output>( 2000, this, 0 );
                                    dao_flag=1;
                                }
                                debug_->debug("node %x (hops= %x) with rank %d rejoined to parrent %x ,%s, %s\n", radio_->id(),rpl_dag_structure.hops,rpl_dag_structure.rank, rpl_dag_structure.parent, rpl_dag_structure.previous_routN ,header.from_routN);
                                dao_counter=0;

                            }
                        }
                    }
                }
            }
            else
            {

                uint16_t i;
                for (i=0; i<=NEIGHBOR_SIZE; i++)
                {
                    if(i==NEIGHBOR_SIZE || i==0xffff)
                    {
                        break;
                    }

                    if(neighbors[i]==header.from_node)
                    {
                        break;
                    }
                }

                if(i!=NEIGHBOR_SIZE)
                {

                    switch(rpl_dag_structure.of)
                    {
                    case MIN_HIGH_TREE:
                        temp_rank=min_high_tree_OF(input_dio.dio_metrics, LQI);
                        // debug_->debug("node %d temp_rank: %d\n",radio_->id(),temp_rank);
                        break;
                    }

                    if(temp_rank<rpl_dag_structure.rank)
                    {

                        rpl_dag_structure.rank = temp_rank;
                        rpl_dag_structure.hops = input_dio.hops + 1;
                        rpl_dag_structure.parent = input_dio.id;
                        rpl_dag_structure.version = input_dio.version;
                        rpl_dag_structure.dag_metrics.metric1 = input_dio.dio_metrics.metric1+1;
                        rpl_dag_structure.root = input_dio.root;
                        if(radio_->id()!=ROOT_NODE && dao_flag!=1)
                        {
                            timer_->set_timer<rpl, &rpl::call_dao_output>( 2000, this, 0 );
                            dao_flag=1;
                        }
                        debug_->debug("node %x (hops= %x) with rank %d joined to parrent %x (metric1= %x)\n", radio_->id(),rpl_dag_structure.hops,rpl_dag_structure.rank, rpl_dag_structure.parent, input_dio.dio_metrics.metric1);
                        dao_counter=0;

                    }
                }
            }
        }
    }
//--------------------------------------------------------------------------------------------------------------------

    uint16_t min_high_tree_OF(metrics_t input_metrics, uint8_t LQI)
    {

        if (input_metrics.metric1==0xffff)
            return input_metrics.metric1;
        else
        {
            uint16_t temp=(input_metrics.metric1+1)*HOP_WEIGHT + LQI * LQI_WEIGHT;
            return temp;
        }
    }
//--------------------------------------------------------------------------------------------------------------------
    void dao_output(rpl_dag_t input_dag)
    {
        if(rpl_dag_structure.joined!=0)
        {
            debug_->debug("node %x tries to send dao to %x\n",radio_->id(), rpl_dag_structure.parent);
            header_t header;
            rpl_dao_t dao;

            header.type=DAO;
            header.from_node = radio_->id();
            header.to_node = input_dag.parent;
            dao.instance_id = input_dag.instance_id;
            //dao.dao_sequence = sequence_counter_;

            uint16_t size;
            size = sizeof(header_t)+sizeof(rpl_dao_t);
            unsigned char message[size];
            memcpy(message, &header, sizeof(header_t));
            memcpy(message+sizeof(header_t),&dao, sizeof(rpl_dao_t));
            radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);

            timer_->set_timer<rpl, &rpl::call_dao_output>( 4000, this, 0 );
        }

    }

//--------------------------------------------------------------------------------------------------------------------
    void call_dao_output(void *)
    {
        if (dao_counter>=0 && dao_counter<=3)
        {
            dao_output(rpl_dag_structure);
            dao_counter++;
        }
    }
//--------------------------------------------------------------------------------------------------------------------
    void dao_response(uint16_t from)
    {

        if(rpl_dag_structure.joined==0)
        {
            int16_t temp = RoutingNumGenerator(from);

            if(temp!=-1)
            {
                header_t header;
                rpl_dao_response_t dao_response;
                header.type=DAO_RESPONSE;
                header.to_node=from;
                header.from_node=radio_->id();

                dao_response.dao_sequence=temp;
                dao_response.instance_id=rpl_dag_structure.instance_id;
                char next[2];
                char hex[]= {"0123456789abcdef"};
                uint8_t i, j;
                i=temp/16;
                j=temp%16;
                sprintf(next,"%c%c",hex[i],hex[j]);
                strcpy(header.to_routN,rpl_dag_structure.routN);
                strcat(header.to_routN,next);

                uint16_t size;
                size = sizeof(header_t)+sizeof(rpl_dao_response_t);
                unsigned char message[size];
                memcpy(message, &header, sizeof(header_t));
                memcpy(message+sizeof(header_t),&dao_response, sizeof(rpl_dao_response_t));
                radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);
                // debug_->debug("process %d with %s received connection question from %d and replied %s\n",radio_->id(), RoutN_, from, mess->Routing);
            }

        }

    }
//--------------------------------------------------------------------------------------------------------------------
    void handle_dao_response (header_t inbox_header, rpl_dao_response_t inbox_dao_res)
    {
        if(inbox_header.from_node==rpl_dag_structure.parent)
        {
            strcpy(rpl_dag_structure.routN, inbox_header.to_routN);
            rpl_dag_structure.joined=0;

            debug_->debug("node %x joined with routN %s\n", radio_->id(), rpl_dag_structure.routN);

            header_t header;
            rpl_dao_response_t dao_res;
            header.type=DAO_ACK;
            header.to_node=inbox_header.from_node;
            header.from_node=radio_->id();

            dao_res.instance_id=rpl_dag_structure.instance_id;
            dao_res.dao_sequence=inbox_dao_res.dao_sequence;

            uint16_t size;
            size = sizeof(header_t)+sizeof(rpl_dao_response_t);
            unsigned char message[size];
            memcpy(message, &header, sizeof(header_t));
            memcpy(message+sizeof(header_t),&dao_res, sizeof(rpl_dao_response_t));
            radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);
            if(needs_repair_==1)
            {
                debug_->debug("node %d rejoined\n", radio_->id());
                send_repair();
            }
        }
    }
//--------------------------------------------------------------------------------------------------------------------
    void handle_dao_ack (header_t inbox_header, rpl_dao_response dao_ack)
    {
        if(pending[dao_ack.dao_sequence][0]==inbox_header.from_node)
        {
            pending[dao_ack.dao_sequence][0]=0xffff;
            given[dao_ack.dao_sequence]=true;
        }
    }
//--------------------------------------------------------------------------------------------------------------------
    void check_pending ( void* )
    {
        //debug_->debug("test\n");
        //repair procedure

        if((parent_timeout_<=PARENT_TIMEOUT) && (rpl_dag_structure.joined==0))
        {

            if ((rpl_dag_structure.joined==0) &&(radio_->id() != ROOT_NODE))
            {
                parent_timeout_++;
               // debug_->debug("node %d increasing parent timeout at %d \n", radio_->id(), parent_timeout_);
            }
        }
        else if((parent_timeout_>PARENT_TIMEOUT) && (rpl_dag_structure.joined==0))
        {
            debug_->debug("node %d lost its parent\n", radio_->id());
            rpl_dag_structure.root=0xffff;

            rpl_dag_structure.rank=0xffff;

            rpl_dag_structure.dag_metrics.metric1=0xffff;

            rpl_dag_structure.joined=0xff;
            strcpy(rpl_dag_structure.previous_routN, rpl_dag_structure.routN);
            // debug_->debug("QUICK DEBUG: %s\n", rpl_dag_structure.routN);

            memset(rpl_dag_structure.routN, NULL, ROUTN_LEN);
            needs_repair_=1;
            ///TODO: Repair();
        }

        for(uint8_t i=0; i<= ROUTN_WIDTH-1; i++)
        {
            if(i==ROUTN_WIDTH-1)
            {
                break;
            }

            if (pending[i][0] != 0xffff)
            {
                if(pending[i][1]>6) //this is for timout of waiting for ack
                {
                    pending[i][1]=0xff;
                    pending[i][0]=0xffff;
                    //               debug_->debug("i1: %x", i);
                }
                else if(pending[i][1]>=4)
                {
                    dao_response(pending[i][0]);
                }

                if (pending[i][1] < 0xff)
                {
                    pending[i][1]++;
                }

            }


        }
        timer_->set_timer<rpl, &rpl::check_pending>( 4000, this, 0 );

    }
//--------------------------------------------------------------------------------------------------------------------
    void data_output ( char dest[ROUTN_LEN], char from[ROUTN_LEN], char input_data[DATA_SIZE])
    {
        //debug_->debug("%x will try to send\n",radio_->id());

        header_t header;
        rpl_data_t rpl_data;

        header.type=DATA;

        if(strncmp(rpl_dag_structure.routN, dest, strlen(rpl_dag_structure.routN))==0)
        {
            //debug_->debug("%s : %s : %d\n",rpl_dag_structure.routN,dest,strlen(rpl_dag_structure.routN));
            //debug_->debug("%d\n",strncmp(rpl_dag_structure.routN, dest, strlen(rpl_dag_structure.routN)));
            //debug_->debug("node %x tries to send DOWN\n",radio_->id());
            rpl_data.direction=DOWN;
        }
        else
        {
            //debug_->debug("node %x tries to send UP\n",radio_->id());
            rpl_data.direction=UP;
        }
        rpl_data.hops=rpl_dag_structure.hops;
        strcpy(header.to_routN,dest);
        strcpy(header.from_routN,from);

        rpl_data.instance_id=rpl_dag_structure.instance_id;
        //strcpy(rpl_data.from, from);
        strcpy(rpl_data.payload,input_data);


        uint16_t size;
        size = sizeof(header_t)+sizeof(rpl_data_t);
        unsigned char message[size];
        memcpy(message, &header, sizeof(header_t));
        memcpy(message+sizeof(header_t),&rpl_data, sizeof(rpl_data_t));
        radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);

    }
//--------------------------------------------------------------------------------------------------------------------
    void data_input(header_t inbox_header, rpl_data_t input_data)
    {
        //debug_->debug("node %d received DATA:1\n",radio_->id());
        //debug_->debug("node %d: debug 1: direction: %d, from: %s, routN %s, routN len: %d, hops: %d\n",radio_->id(),input_data.direction,inbox_header.from_routN,rpl_dag_structure.routN,strlen(rpl_dag_structure.routN),input_data.hops );
        if((input_data.direction==UP) && (strncmp(rpl_dag_structure.routN, inbox_header.from_routN,strlen(rpl_dag_structure.routN))==0) && (input_data.hops==rpl_dag_structure.hops+1))
            //if ((strncmp(inbox_header.from_routN, inbox_header.to_routN, strlen(rpl_dag_structure.routN)+2)!=0) && (strlen(inbox_header.from_routN)==strlen(rpl_dag_structure.routN)+2))
        {
            // debug_->debug("node %d received DATA:2\n",radio_->id());
            if(strcmp(inbox_header.to_routN,rpl_dag_structure.routN)==0)
            {
                //debug_->debug("received DATA to %s, from: %s, data: %s\n", rpl_dag_structure.routN,inbox_header.from_routN, input_data.payload);
                //debug_->debug("received DATA to %s, data: %s\n", rpl_dag_structure.routN, input_data.payload);
            }
            else //if(strncmp(inbox_header.from_routN, rpl_dag_structure.routN, strlen(rpl_dag_structure.routN))==0)
            {
                // debug_->debug("resending from %s\n",rpl_dag_structure.routN);
                data_output(inbox_header.to_routN, inbox_header.from_routN, input_data.payload);
            }
        }
        else if((input_data.direction==DOWN) && (strncmp(rpl_dag_structure.routN, inbox_header.to_routN, strlen(rpl_dag_structure.routN))==0) && (input_data.hops==rpl_dag_structure.hops-1))
            //else if((strncmp(inbox_header.from_routN, rpl_dag_structure.routN, strlen(rpl_dag_structure.routN)-2)==0) && (strlen(inbox_header.from_routN)==strlen(rpl_dag_structure.routN)-2))
        {
            if(strcmp(inbox_header.to_routN,rpl_dag_structure.routN)==0)
            {
                //debug_->debug("received DATA to %s, from: %s, data: %s\n", rpl_dag_structure.routN,inbox_header.from_routN, input_data.payload);
                //debug_->debug("received DATA to %s, data: %s\n", rpl_dag_structure.routN, input_data.payload);
            }
            else //if(strncmp(inbox_header.to_routN, rpl_dag_structure.routN, strlen(rpl_dag_structure.routN))==0)
            {
                // debug_->debug("resending from %s\n",rpl_dag_structure.routN);
                data_output(inbox_header.to_routN, inbox_header.from_routN ,input_data.payload);
            }
        }
    }
//--------------------------------------------------------------------------------------------------------------------
    void send_repair()
    {
        if(strlen(rpl_dag_structure.routN)==ROUTN_LEN)
        {
            //SEND disband message
            send_disband();
        }
        else
        {
            //generate new routN for each child.
            for(uint8_t i=0; i<ROUTN_WIDTH; i++)
            {
                if(i==0xff)
                {
                    break;
                }
                if (given[i]==true)
                {
                    //send message
                    header_t header;
                    header.type=REPAIR;
                    header.from_node=radio_->id();

                    uint8_t j, k;
                    char next[2];
                    char hex[]= {"0123456789abcdef"};

                    j=i/16;
                    k=i%16;

                    sprintf(next,"%c%c",hex[j],hex[k]);
                    strcpy(header.from_routN, rpl_dag_structure.routN);
                    strcat(header.from_routN,next);
                    strcpy(header.to_routN,rpl_dag_structure.previous_routN);
                    strcat(header.to_routN,next);

                    uint16_t size;
                    size = sizeof(header_t);
                    unsigned char message[size];
                    memcpy(message, &header, sizeof(header_t));

                    radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), message);
                }
            }
            debug_->debug("node %d send repair\n", radio_->id());
        }
    }

//--------------------------------------------------------------------------------------------------------------------

    void handle_repair(header_t inbox_header)
    {

        if(strncmp(rpl_dag_structure.routN, inbox_header.to_routN, strlen(rpl_dag_structure.routN))==0 && (inbox_header.from_node==rpl_dag_structure.parent))
        {
            //change prefix
            strcpy(rpl_dag_structure.previous_routN, rpl_dag_structure.routN);
            memset(rpl_dag_structure.routN, NULL, ROUTN_LEN);
            strcpy(rpl_dag_structure.routN, inbox_header.from_routN);
            debug_->debug("node %d repaired\n", radio_->id());
            debug_->debug("node %d received repair from %d, with to_routN: %s, from_routN: %s\n", radio_->id(), inbox_header.from_node, inbox_header.to_routN, inbox_header.from_routN);
            send_repair();
        }
    }

//--------------------------------------------------------------------------------------------------------------------
    void send_disband ()
    {
        for(uint8_t i=0; i<ROUTN_WIDTH; i++)
        {
            if(i==0xff)
            {
                break;
            }
            if (given[i]==true)
            {
                header_t header;
                header.type=DISBAND;
                header.from_node=radio_->id();

                uint8_t j, k;
                char next[2];
                char hex[]= {"0123456789abcdef"};

                j=i/16;
                k=i%16;

                sprintf(next,"%c%c",hex[j],hex[k]);
                strcpy(header.from_routN, rpl_dag_structure.routN);
                strcpy(header.to_routN,rpl_dag_structure.routN);
                strcat(header.to_routN,next);

                uint16_t size;
                size = sizeof(header_t);
                unsigned char message[size];
                memcpy(message, &header, sizeof(header_t));

                radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), message);
            }
        }
        debug_->debug("node %d send disband\n", radio_->id());

    }
//--------------------------------------------------------------------------------------------------------------------
    void handle_disband ( header_t inbox_header )
    {
        if(strncmp(rpl_dag_structure.routN, inbox_header.to_routN, strlen(rpl_dag_structure.routN))==0 && (inbox_header.from_node==rpl_dag_structure.parent))
        {
            rpl_dag_structure.root=0xffff;
            rpl_dag_structure.rank=0xffff;
            rpl_dag_structure.dag_metrics.metric1=0xffff;
            rpl_dag_structure.joined=0xff;
            rpl_dag_structure.of=OF_TO_USE;
            memset(rpl_dag_structure.routN, NULL, ROUTN_LEN);
            memset(rpl_dag_structure.previous_routN, NULL, ROUTN_LEN);

            send_disband();
        }

    }
//--------------------------------------------------------------------------------------------------------------------
    void test_mess( void* )
    {
        for(message_counter=0;message_counter<100000;message_counter++){
        data_output("0001000000",rpl_dag_structure.routN,"test_data");
        }
    }
//--------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------
    void receive_radio_message( Os::Radio::node_id_t from, Os::Radio::size_t len, Os::Radio::block_data_t *buf,  const Os::Radio::ExtendedData& exdata )
    {
        header_t header;
        memcpy(&header, buf, sizeof(header_t));

        switch(header.type)
        {
        case NEIGHBOR_DIS:
            handle_neighbor_discovery(header);
            // free(header);
            //TODO: complete this
            break;
        case NEIGHBOR_DIS_ACK:
            if(header.to_node==radio_->id())
            {
                handle_neighbor_discovery_ack(header);
            }
            break;
        case DIO:
            if(header.to_node==radio_->id())
            {
                rpl_dio_t temp_dio;
                memcpy(&temp_dio, buf+sizeof(header_t), sizeof(rpl_dio_t));
                dio_input(header, temp_dio, exdata.link_metric());
            }
            break;
        case DAO:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node %x received DAO from %x\n", radio_->id(),from);
                //rpl_dao_t temp_dao;
                //memcpy(&temp_dao, buf+sizeof(header_t), sizeof(rpl_dao_t));
                dao_response(header.from_node);
            }
            break;
        case DAO_RESPONSE:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node %x received DAO_RES from %x\n", radio_->id(),from);
                rpl_dao_response_t temp_dao_res;
                memcpy(&temp_dao_res, buf+sizeof(header_t), sizeof(rpl_dao_response_t));
                handle_dao_response(header, temp_dao_res);
            }
            break;
        case DAO_ACK:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node %x received DAO_ACK from %x\n", radio_->id(),from);
                rpl_dao_response_t temp_dao_ack;
                memcpy(&temp_dao_ack, buf+sizeof(header_t), sizeof(rpl_dao_response_t));
                handle_dao_ack(header,temp_dao_ack);
            }
            break;
        case DATA:
            //debug_->debug("node %x received DATA from %x\n", radio_->id(),from);
            rpl_data_t temp_data;
            memcpy(&temp_data, buf+sizeof(header_t), sizeof(rpl_data_t));
            data_input(header,temp_data);
            break;
        case REPAIR:
            handle_repair(header);
            break;
        case DISBAND:
            handle_disband(header);
            break;
        }
    }
//--------------------------------------------------------------------------------------------------------------------

    int16_t RoutingNumGenerator (uint16_t from)
    {

        for(uint8_t i=0; i<=ROUTN_WIDTH-1; i++)
        {
            if(i==ROUTN_WIDTH-1)
                break;

            if (pending[i][0]==from)
                return(i);
        }

        for (uint8_t i=0; i<=ROUTN_WIDTH-1; i++)
        {
            if(i==ROUTN_WIDTH-1)
            {
                break;
            }
            if(given[i]==false && pending[i][0]==0xffff)
            {
                pending[i][0]=from;
                return(i);

            }
        }
        return (-1);

    }
//--------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------
private:
    Os::Radio::self_pointer_t radio_;
    Os::Timer::self_pointer_t timer_;
    Os::Debug::self_pointer_t debug_;
    rpl_dag_t rpl_dag_structure;
    uint16_t neighbors[NEIGHBOR_SIZE];
    uint8_t sequence_counter_;
    uint8_t parent_timeout_;
    uint16_t temp_rank;
    uint8_t dao_flag;
    uint32_t message_counter;
    uint8_t needs_repair_;
    uint8_t power;
    uint8_t dao_counter;
    bool given[ROUTN_WIDTH];
    uint16_t pending[ROUTN_WIDTH][2];
};
// --------------------------------------------------------------------------
wiselib::WiselibApplication<Os, rpl> rpl_app;
// --------------------------------------------------------------------------
void application_main( Os::AppMainParameter& value )
{
    rpl_app.init( value );
}
