

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

#define ROOT_NODE 0x296
#define SEND_TO 0xca3
#define SEND_FROM 0x1cde

#define NEIGHBOR_SIZE 40 //65534 is the maximum value. do NOT set this bigger than 255 never!
#define NEIGHBOR_WIDTH 40
#define SENT_QUEUE 16      //254 is the maximum value. do NOT set this bigger than 255 never!

#define OF_TO_USE 0x00
#define MIN_HIGH_TREE 0x00
#define DATA_SIZE 32

#define NEIGHBOR_DIS 0
#define NEIGHBOR_DIS_ACK 1
#define DIO 2
#define DAO 3
#define DAO_ACK 4
#define DATA 5

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
        char from[32];
        char Routing[32];
        metrics_t dio_metrics;

    };
    typedef rpl_dio rpl_dio_t;

    struct header
    {
        uint8_t type;
        uint16_t to_node;
        uint16_t from_node;
       // uint16_t to_ipv6[8];
      //  uint16_t from_ipv6[8];
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
        metrics_t dag_metrics;
    };
    typedef rpl_dag rpl_dag_t;


    struct rpl_dao
    {
        uint16_t to;
        uint16_t from;
        uint8_t instance_id;
        uint8_t dao_sequence;
    };

        typedef rpl_dao rpl_dao_t;

    struct rpl_dao_ack
    {
        uint16_t to;
        uint16_t from;
        uint8_t instance_id;
        uint8_t dao_sequence;
    };

    typedef rpl_dao_ack rpl_dao_ack_t;

    struct rpl_data
    {
        uint16_t to;
        uint16_t from;
        uint8_t instance_id;
        unsigned char payload[DATA_SIZE];
    };

    typedef rpl_data rpl_data_t;

    void init( Os::AppMainParameter& value )
    {
        radio_ = &wiselib::FacetProvider<Os, Os::Radio>::get_facet( value );
        timer_ = &wiselib::FacetProvider<Os, Os::Timer>::get_facet( value );
        debug_ = &wiselib::FacetProvider<Os, Os::Debug>::get_facet( value );



        radio_->reg_recv_callback<rpl,
        &rpl::receive_radio_message>( this );

        timer_->set_timer<rpl, &rpl::discover_neighbors>( 2000, this, 0 );

        rpl_dag_structure.root=0xffff;

        rpl_dag_structure.rank=0xffff;

        rpl_dag_structure.dag_metrics.metric1=0xffff;

        rpl_dag_structure.of=OF_TO_USE;

       for(uint16_t j=0;j<NEIGHBOR_SIZE; j++)
       {
            for (uint16_t i=0;i<NEIGHBOR_WIDTH; i++)
            {
                // debug_->debug("node: %x value: %x",radio_->id(), i);
                neighbors[j][i]=0xffff;


            }
       }
         //debug_->debug("%x",rpl_dag_structure.of);
        for (uint8_t i=0;i<=SENT_QUEUE;i++)
        {
            sent_timer_[i]=0xff;
            if(i==0xff)
            {
                break;
            }
        }


        sequence_counter_=0x00;

        if(radio_->id()==ROOT_NODE)
        {
            rpl_dag_structure.rank=0;
            rpl_dag_structure.root=radio_->id();

            rpl_dag_structure.dag_metrics.metric1=0;
            rpl_dag_structure.version=5;

            debug_->debug("IM ROOT");

        }
        if(radio_->id()==SEND_FROM)
        {
           timer_->set_timer<rpl, &rpl::send>( 60000, this, 0 );
        }

                    timer_->set_timer<rpl,
        &rpl::dio_output>( 15000, this, 0 );

           timer_->set_timer<rpl, &rpl::check_sent_timer>( 4000, this, 0 );

    }
//--------------------------------------------------------------------------------------------------------------------
  //  void discover_neighbors( void* )
//--------------------------------------------------------------------------------------------------------------------
    void discover_neighbors( void* )
    {
        header_t my_header;
        my_header.type=NEIGHBOR_DIS;
        my_header.from_node=radio_->id();
        radio_->send( Os::Radio::BROADCAST_ADDRESS, sizeof(header_t), ( Os::Radio::block_data_t*)&my_header );
       // timer_->set_timer<rpl, &rpl::discover_neighbors>( 2000, this, 0 );
    }

//--------------------------------------------------------------------------------------------------------------------

    void handle_neighbor_discovery (header_t inbox_header)
    {
      //  debug_->debug("node %x received nd from node %x",radio_->id(), inbox_header.from_ipv6[0]);
        header_t header;
        header.type=NEIGHBOR_DIS_ACK;
       // header.to_ipv6[0]=inbox_header.from_ipv6[0];
       // header.from_ipv6[0]=radio_->id();

       // test
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
            if (position==0xffff && neighbors[i][0]==0xffff)
            {
                position=i;
            }
            if(neighbors[i][0]==inbox_header.from_node)
            {
                used=true;
                break;
            }
        }
        if(used==false)
        {
            neighbors[position][0]=inbox_header.from_node;
            debug_->debug("node: %x added %x to list\n", radio_->id(), inbox_header.from_node);
        }
    }
//--------------------------------------------------------------------------------------------------------------------
void dio_output (void *)
{
    rpl_dio_t dio;

    dio.id = radio_->id();
    dio.dio_metrics = rpl_dag_structure.dag_metrics;
    dio.version = rpl_dag_structure.version;
    dio.instance_id = rpl_dag_structure.instance_id;
    dio.root = rpl_dag_structure.root;

    header_t header;
    header.type=DIO;
    header.from_node=radio_->id();
    for (uint16_t i=0;i<=NEIGHBOR_SIZE;i++)
        {
        if(neighbors[i][0]==0xffff)
        {
            break;
        }
        else
        {
            header.to_node=neighbors[i][0];
        }
        uint16_t size;
        size = sizeof(header_t)+sizeof(rpl_dio_t);
        unsigned char message[size];
        memcpy(message, &header, sizeof(header_t));
        memcpy(message+sizeof(header_t),&dio, sizeof(rpl_dio_t));
   // debug_->debug("metric1: %x, version: %x", dio.dio_metrics.metric1, dio.version);

        radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);
   }

    timer_->set_timer<rpl, &rpl::dio_output>( 2000, this, 0 );

}

//--------------------------------------------------------------------------------------------------------------------
void dio_input (header_t header, rpl_dio_t input_dio)
{
    uint16_t i;
          for (i=0;i<=NEIGHBOR_SIZE;i++)
          {
              if(i==NEIGHBOR_SIZE || i==0xffff)
              {
                  break;
              }

              if(neighbors[i][0]==header.from_node)
              {
                  break;
              }
          }

          if(i!=NEIGHBOR_SIZE)
          {

              switch(rpl_dag_structure.of)
            {
                case MIN_HIGH_TREE:
                temp_rank=min_high_tree_OF(input_dio.dio_metrics);
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
                if(radio_->id()!=ROOT_NODE){
                timer_->set_timer<rpl, &rpl::send_dao>( 1000, this, 0 );
                }
                debug_->debug("node %x (metric1= %x) joined to parrent %x (metric1= %x)\n", radio_->id(),rpl_dag_structure.dag_metrics.metric1, rpl_dag_structure.parent, input_dio.dio_metrics.metric1);
            }
          }
}
//--------------------------------------------------------------------------------------------------------------------
void dao_output (rpl_dag_t input_dag, uint16_t from)
{


   for (sequence_counter_=0;sequence_counter_<=SENT_QUEUE;sequence_counter_++)
   {
              if(sequence_counter_==0xff || sequence_counter_==SENT_QUEUE)
                {
                    break;
                }
       if(sent_node_[sequence_counter_]==from && sent_node_[sequence_counter_]!=0xffff)
       {
           debug_->debug("res1 node %x needs to resend to node %x from %x\n", radio_->id(), input_dag.parent, from);
           break;

       }

   }

   if(sequence_counter_==SENT_QUEUE)
   {
        for (sequence_counter_=0;sequence_counter_<=SENT_QUEUE;sequence_counter_++)
            {
                if(sequence_counter_==SENT_QUEUE || sequence_counter_==0xff)
                {
                    break;
                }
                if(sent_timer_[sequence_counter_]==0xff)
                {

                    break;
                }

            }
   }
    if (sequence_counter_!=SENT_QUEUE)
    {
        debug_->debug("res2 node %x needs to resend to node %x from %x\n", radio_->id(), input_dag.parent, from);

        header_t header;
        rpl_dao_t dao;

        header.type=DAO;
        header.from_node = radio_->id();
        header.to_node = input_dag.parent;
       // debug_->debug("parent: %x", sequence_counter_  );

        dao.to=input_dag.root;
        dao.from=from;


        dao.instance_id = input_dag.instance_id;
        dao.dao_sequence = sequence_counter_;
        sent_timer_[sequence_counter_]=0;
        sent_node_[sequence_counter_]=from;

        uint16_t size;
        size = sizeof(header_t)+sizeof(rpl_dao_t);
        unsigned char message[size];
        memcpy(message, &header, sizeof(header_t));
        memcpy(message+sizeof(header_t),&dao, sizeof(rpl_dao_t));
        radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);




    }
}
//--------------------------------------------------------------------------------------------------------------------
void dao_input(header_t header, rpl_dao_t input_dao)
{
   // if(input_dao.to==radio_->id())                      //einai gia tin parousa diergasia to dao?
  //  {
        //handle dao o.O
 //   }
 //   else
  //  {
  if (input_dao.instance_id == rpl_dag_structure.instance_id)
  {
        send_dao_ack(header.from_node, input_dao.dao_sequence);
        debug_->debug("sending DAO_ACK back to: %x\n",header.from_node);
        uint16_t position;

        for (position=0;position<=NEIGHBOR_SIZE; position++)  // briskei ton geitona pou to esteile
        {
            if(neighbors[position][0]==header.from_node)
            {
                //position=i;
                neighbors[position][1]=header.from_node;
                debug_->debug("neighbor: %x",neighbors[position][0]);
                break;
            }

            if(position==NEIGHBOR_SIZE)
            {
                break;
            }
        }

        if (position!=NEIGHBOR_SIZE)                        //psaxnei na brei an to i diergasia pou proorizete to dao uparxei sto rout table. an oxi, tin prosthetei.
        {
            uint16_t position_2;
            uint16_t temp=0xffff;
            for (position_2=2;position_2<=NEIGHBOR_SIZE; position_2++)
            {
                if(neighbors[position][position_2]==0xffff && temp==0xffff)
                {
                    //neighbors[position][position_2]=input_dao.from;
                   // break;
                   temp=position_2;
                }
                else if (neighbors[position][position_2]==input_dao.from)
                {
                    //debug_->debug("neighbors[position][position_2]: %x",neighbors[position][position_2]);
                    break;
                }

                if(position_2==NEIGHBOR_SIZE)
                {
                    break;
                }
            }

            if (position_2==NEIGHBOR_SIZE)
            {
                 neighbors[position][temp]=input_dao.from;
                 debug_->debug("added: %x in position: %x and postion_2: %x\n",neighbors[position][temp],position,temp);
            }
        }
        if (radio_->id()!=input_dao.to)
        {
            dao_output(rpl_dag_structure, input_dao.from);
        }
  }
   // }
}
//--------------------------------------------------------------------------------------------------------------------
void send_dao_ack (uint16_t dest, uint8_t sequence)
{
    header_t header;
    rpl_dao_ack_t dao_ack;
    header.type = DAO_ACK;
    header.to_node = dest;
    header.from_node = radio_->id();
    dao_ack.dao_sequence = sequence;
    dao_ack.instance_id = rpl_dag_structure.instance_id;

    uint16_t size;
    size = sizeof(header_t)+sizeof(rpl_dao_t);
    unsigned char message[size];
    memcpy(message, &header, sizeof(header_t));
    memcpy(message+sizeof(header_t),&dao_ack, sizeof(rpl_dao_ack_t));
    radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);
}

//--------------------------------------------------------------------------------------------------------------------
void handle_dao_ack (header_t header, rpl_dao_ack_t dao_ack)
{
    if(sent_timer_[dao_ack.dao_sequence]!=0xff)
    {
        sent_timer_[dao_ack.dao_sequence]=0xff;
        sent_node_[dao_ack.dao_sequence]=0xffff;
    }
}
//--------------------------------------------------------------------------------------------------------------------
void clear_sent_timer (uint8_t sequence)
{
    sent_timer_[sequence_counter_]=0xff;
}
//--------------------------------------------------------------------------------------------------------------------
void check_sent_timer ( void* )
{


    for(uint8_t i=0; i<= SENT_QUEUE; i++)
    {
            if(i==SENT_QUEUE)
            {
                break;
            }

        if (sent_timer_[i] != 0xff)
        {
            if(sent_timer_[i]>6) //this is for timout of waiting for ack
            {
                sent_timer_[i]=0xff;
                sent_node_[i]=0xffff;
            }
            else if(sent_timer_[i]>=4)
            {
                dao_output(rpl_dag_structure, sent_node_[i]);
            }

            if (sent_timer_[i] < 0xff)
            {
                sent_timer_[i]++;
            }

        }


    }
    timer_->set_timer<rpl, &rpl::check_sent_timer>( 4000, this, 0 );
}
//--------------------------------------------------------------------------------------------------------------------
void send_dao( void* )
{
    dao_output(rpl_dag_structure, radio_->id());
}

//--------------------------------------------------------------------------------------------------------------------
void data_output ( uint16_t dest, uint16_t from, unsigned char input_data[DATA_SIZE])
{
    debug_->debug("%x will try to send\n",radio_->id());
    if (dest!=radio_->id())
    {

    header_t header;
    rpl_data_t data;

    header.type=DATA;
    data.to = dest;
    data.instance_id=rpl_dag_structure.instance_id;
    if(from==radio_->id())
    {
        data.from=radio_->id();
    }
    else
    data.from=from;

    header.from_node = radio_->id();
    header.to_node=0xffff; //flag (o Thanasis tha me misei gia ta tromera sxolia m)


    for (uint16_t i=0; i<=NEIGHBOR_SIZE; i++)
    {
       // if(i==0xffff)
       // break;
       if(header.to_node!=0xffff)
       break;

       if (neighbors[i][0]==0xffff || dest== rpl_dag_structure.parent)
       {
           if(radio_->id()!=rpl_dag_structure.root)
           {
                header.to_node=rpl_dag_structure.parent;
                break;
           }
       }
       else
       {
           for (uint16_t j=1;j<=NEIGHBOR_SIZE; j++)
           {
                if(neighbors[i][j]==0xffff)
                break;

                if(neighbors[i][j]==dest)
                {
                    header.to_node=neighbors[i][0];
                    break;
                }

           }
       }
    }


    if(header.to_node!=0xffff)
    {
        debug_->debug("data.to: %x, data.from: %x, header.to: %x, header.from: %x\n", data.to, data.from, header.to_node, header.from_node);
        memcpy(data.payload, input_data, DATA_SIZE);
        uint16_t size;
        size = sizeof(header_t)+sizeof(rpl_data_t);
        unsigned char message[size];
        memcpy(message, &header, sizeof(header_t));
        memcpy(message+sizeof(header_t),&data, sizeof(rpl_data_t));
        radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);
    }
    else
    debug_->debug("found none\n");
    }

}
//--------------------------------------------------------------------------------------------------------------------
void data_input(rpl_data_t data)
{
    if(data.instance_id==rpl_dag_structure.instance_id)
    {
        if(data.to==radio_->id())
        {
            //handle payload
            debug_->debug("node %x received DATA from %x\n", radio_->id(), data.from);

        }
        else
        {
            data_output(data.to, data.from, data.payload);
        }
    }
}
//--------------------------------------------------------------------------------------------------------------------
void send ( void* )
{
    data_output(SEND_TO, radio_->id(),(unsigned char *)"test_data");
}
//--------------------------------------------------------------------------------------------------------------------

uint16_t min_high_tree_OF(metrics_t input_metrics)
{

    if (input_metrics.metric1==0xffff)
    return input_metrics.metric1;
    else
    return input_metrics.metric1+1;
}
//--------------------------------------------------------------------------------------------------------------------
    void receive_radio_message( Os::Radio::node_id_t from, Os::Radio::size_t len, Os::Radio::block_data_t *buf, const Os::Radio::ExtendedData& exdata )
    {
        //debug_->debug("LQI: %d", 255 - exdata.link_metric());
        header_t header;
        memcpy(&header, buf, sizeof(header_t));

        switch(header.type)
        {
        case NEIGHBOR_DIS:
            handle_neighbor_discovery(header);
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
                dio_input(header, temp_dio);
            }
            break;
        case DAO:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node: %x received dao from: %x\n",radio_->id(),header.from_node);
                rpl_dao_t temp_dao;
                memcpy(&temp_dao, buf+sizeof(header_t), sizeof(rpl_dao_t));
                dao_input(header, temp_dao);
            }
            break;
        case DAO_ACK:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node: %x received DAO_ACK from: %x\n",radio_->id(),header.from_node);
                rpl_dao_ack_t temp_dao_ack;
                memcpy(&temp_dao_ack, buf+sizeof(header_t), sizeof(rpl_dao_ack_t));
                handle_dao_ack(header, temp_dao_ack);
            }
            break;
        case DATA:
            if(header.to_node==radio_->id())
            {

                rpl_data_t temp_data;
                memcpy(&temp_data, buf+sizeof(header_t), sizeof(rpl_data_t));
                data_input(temp_data);
            }
        }
    }
//--------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------
private:
        Os::Radio::self_pointer_t radio_;
        Os::Timer::self_pointer_t timer_;
        Os::Debug::self_pointer_t debug_;

        uint16_t neighbors[NEIGHBOR_SIZE][NEIGHBOR_WIDTH];
        uint8_t hops_;
        uint16_t temp_rank;
        rpl_dag_t rpl_dag_structure;
        uint8_t sequence_counter_;
        uint8_t sent_timer_[SENT_QUEUE];
        uint16_t sent_node_[SENT_QUEUE];
    };
// --------------------------------------------------------------------------
    wiselib::WiselibApplication<Os, rpl> rpl_app;
// --------------------------------------------------------------------------
    void application_main( Os::AppMainParameter& value )
    {
        rpl_app.init( value );
    }
