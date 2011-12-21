/*
 * Simple Wiselib Example
 */
#include "external_interface/external_interface.h"
#include "algorithms/routing/tree/tree_routing.h"


#define NEIGHBOR_SIZE 255 //65534 is the maximum value. do NOT set this bigger than 255 never!
#define SENT_QUEUE 0xff      //254 is the maximum value. do NOT set this bigger than 255 never!

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
            for (uint16_t i=0;i<NEIGHBOR_SIZE; i++)
            {
                // debug_->debug("node: %d value: %d\n",radio_->id(), i);
                neighbors[j][i]=0xffff;


            }
       }
         debug_->debug("%d\n",rpl_dag_structure.of);
        for (uint8_t i=0;i<=SENT_QUEUE;i++)
        {
            sent_timer_[i]=0xff;
            if(i==0xff)
            {
                break;
            }
        }


        sequence_counter_=0x00;

        if(radio_->id()==0)
        {
            rpl_dag_structure.rank=0;
            rpl_dag_structure.root=radio_->id();

            rpl_dag_structure.dag_metrics.metric1=0;
            rpl_dag_structure.version=5;

        }
        if(radio_->id()==6)
        {
           timer_->set_timer<rpl, &rpl::send_dao>( 15000, this, 0 );
        }

                    timer_->set_timer<rpl,
        &rpl::dio_output>( 5000, this, 0 );

           timer_->set_timer<rpl, &rpl::check_sent_timer>( 1000, this, 0 );

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

    }

//--------------------------------------------------------------------------------------------------------------------

    void handle_neighbor_discovery (header_t inbox_header)
    {
      //  debug_->debug("node %d received nd from node %d\n",radio_->id(), inbox_header.from_ipv6[0]);
        header_t header;
        header.type=NEIGHBOR_DIS_ACK;
       // header.to_ipv6[0]=inbox_header.from_ipv6[0];
       // header.from_ipv6[0]=radio_->id();
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
            debug_->debug("node: %d added %d to list\n", radio_->id(), inbox_header.from_node);
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
    uint16_t size;
    size = sizeof(header_t)+sizeof(rpl_dio_t);
    unsigned char message[size];
    memcpy(message, &header, sizeof(header_t));
    memcpy(message+sizeof(header_t),&dio, sizeof(rpl_dio_t));
   // debug_->debug("metric1: %d, version: %d\n", dio.dio_metrics.metric1, dio.version);

    radio_->send( Os::Radio::BROADCAST_ADDRESS, size, message);

    timer_->set_timer<rpl, &rpl::dio_output>( 2000, this, 0 );

}

//--------------------------------------------------------------------------------------------------------------------
void dio_input (rpl_dio_t input_dio)
{
            switch(rpl_dag_structure.of)
            {
                case MIN_HIGH_TREE:
                temp_rank=min_high_tree_OF(input_dio.dio_metrics);
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
                debug_->debug("node %d (metric1= %d) joined to parrent %d (metric1= %d)\n", radio_->id(),rpl_dag_structure.dag_metrics.metric1, rpl_dag_structure.parent, input_dio.dio_metrics.metric1);
   }
}
//--------------------------------------------------------------------------------------------------------------------
void dao_output (rpl_dag_t input_dag, uint16_t from)
{


   for (sequence_counter_=0;sequence_counter_<=SENT_QUEUE;sequence_counter_++)
   {
       if(sent_node_[sequence_counter_]==from && sent_node_[sequence_counter_]!=0xffff)
       {
           debug_->debug("node %d needs to resend from node %d\n", radio_->id(), from);
           break;

       }
       if(sequence_counter_==0xff)
                {
                    break;
                }
   }

   if(sequence_counter_==0xff)
   {
        for (sequence_counter_=0;sequence_counter_<=SENT_QUEUE;sequence_counter_++)
            {
                if(sequence_counter_==SENT_QUEUE)
                {
                    break;
                }
                if(sent_timer_[sequence_counter_]==0xff)
                {

                    break;
                }

            }
//   }
    if (sequence_counter_!=SENT_QUEUE)
    {

        header_t header;
        rpl_dao_t dao;

        header.type=DAO;
        header.from_node = radio_->id();
        header.to_node = input_dag.parent;
        debug_->debug("parent: %d\n", sequence_counter_  );

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
        debug_->debug("sending DAO_ACK back to: %d\n",header.from_node);
        uint16_t position;

        for (position=0;position<=NEIGHBOR_SIZE; position++)  // briskei ton geitona pou to esteile
        {
            if(neighbors[position][0]==header.from_node)
            {
                //position=i;
                debug_->debug("neighbor: %d\n",neighbors[position][0]);
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
            for (position_2=0;position_2<=NEIGHBOR_SIZE; position_2++)
            {
                if(neighbors[position][position_2]==0xffff && temp==0xffff)
                {
                    //neighbors[position][position_2]=input_dao.from;
                   // break;
                   temp=position_2;
                }
                else if (neighbors[position][position_2]==input_dao.from)
                {
                    //debug_->debug("neighbors[position][position_2]: %d\n",neighbors[position][position_2]);
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
                 debug_->debug("added: %d in position: %d and postion_2: %d\n",neighbors[position][temp],position,temp);
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
                 //               debug_->debug("i1: %d\n", i);
            }
            else if(sent_timer_[i]>=4)
            {
                dao_output(rpl_dag_structure, sent_node_[i]);
                //debug_->debug("sent_timer: %d\n", sent_timer_[i]);
               // debug_->debug("i2: %d\n", i);
            }

            if (sent_timer_[i] < 0xff)
            {
                sent_timer_[i]++;
            }

        }


    }
    timer_->set_timer<rpl, &rpl::check_sent_timer>( 1000, this, 0 );
}
//--------------------------------------------------------------------------------------------------------------------
void send_dao( void* )
{
    dao_output(rpl_dag_structure, radio_->id());
}

//--------------------------------------------------------------------------------------------------------------------
void data_output ( uint16_t dest, unsigned char input_data[DATA_SIZE])
{
    header_t header;
    rpl_data_t data;

    data.to = dest;
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
    void receive_radio_message( Os::Radio::node_id_t from, Os::Radio::size_t len, Os::Radio::block_data_t *buf )
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
            rpl_dio_t temp_dio;
            memcpy(&temp_dio, buf+sizeof(header_t), sizeof(rpl_dio_t));
            dio_input(temp_dio);
            break;
        case DAO:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node: %d received dao from: %d\n",radio_->id(),header.from_node);
                rpl_dao_t temp_dao;
                memcpy(&temp_dao, buf+sizeof(header_t), sizeof(rpl_dao_t));
                dao_input(header, temp_dao);
            }
            break;
        case DAO_ACK:
            if(header.to_node==radio_->id())
            {
                debug_->debug("node: %d received DAO_ACK from: %d\n",radio_->id(),header.from_node);
                rpl_dao_ack_t temp_dao_ack;
                memcpy(&temp_dao_ack, buf+sizeof(header_t), sizeof(rpl_dao_ack_t));
                handle_dao_ack(header, temp_dao_ack);
            }
        }
    }
//--------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------
private:
        Os::Radio::self_pointer_t radio_;
        Os::Timer::self_pointer_t timer_;
        Os::Debug::self_pointer_t debug_;

        uint16_t neighbors[NEIGHBOR_SIZE][NEIGHBOR_SIZE];
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
