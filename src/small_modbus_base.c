/*
 * Change Logs:
 * Date           Author       Notes
 * 2020-08-21     chenbin      small modbus the first version
 */
#include "small_modbus.h"

int _modbus_init(small_modbus_t *smb)
{
	if(smb != NULL)
	{
		smb->transfer_id = 0;
		smb->protocol_id = 0;
		smb->debug_level = 0;
		if(smb->timeout_frame==0)
		{
				smb->timeout_frame = 100;
		}
		if(smb->timeout_byte==0)
		{
				smb->timeout_byte = 10;
		}
	}
	return MODBUS_OK;
}
/*
 * *
 */
int modbus_connect(small_modbus_t *smb)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->open != NULL))
	{
			return smb->port->open(smb);
	}
	return MODBUS_FAIL;
}

int modbus_disconnect(small_modbus_t *smb)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->close != NULL))
	{
			return smb->port->close(smb);
	}
	return MODBUS_FAIL;
}

int modbus_write(small_modbus_t *smb,uint8_t *data,uint16_t length)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->write != NULL))
	{
			return smb->port->write(smb,data,length);
	}
	return MODBUS_FAIL;
}

int modbus_read(small_modbus_t *smb,uint8_t *data,uint16_t length)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->read != NULL))
	{
			return smb->port->read(smb,data,length);
	}
	return MODBUS_FAIL;
}

int modbus_flush(small_modbus_t *smb)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->flush != NULL))
	{
			return smb->port->flush(smb);
	}
	return MODBUS_FAIL;
}

int modbus_wait(small_modbus_t *smb,int timeout)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->wait != NULL))
	{
			return smb->port->wait(smb,timeout);
	}
	return MODBUS_FAIL;
}

int modbus_want_read(small_modbus_t *smb,uint8_t *buff,uint16_t len,int32_t wait_time)
{
	uint8_t *read_buff = buff;
	uint16_t read_len = 0;
	int rc = 0;
	
	do{
		read_len += modbus_read(smb,read_buff+read_len , len - read_len);
		if(read_len >= len)
		{
			return read_len; //read ok
		}
		rc = modbus_wait(smb,wait_time);
	}while(rc >= MODBUS_OK);
	
	return rc; //happen An error occurred
}

int modbus_error_recovery(small_modbus_t *smb)
{
	if((smb != NULL)&&(smb->port != NULL)&&(smb->port->flush != NULL))
	{
			return smb->port->flush(smb);
	}
	return MODBUS_FAIL;
}

int modbus_error_exit(small_modbus_t *smb,int code)
{
	if(smb!=NULL)
	{
			smb->error_code = code;
	}
	return code;
}

int modbus_set_frame_timeout(small_modbus_t *smb,int timeout_ms)
{
    smb->timeout_frame = timeout_ms;
    return MODBUS_OK;
}

int modbus_set_byte_timeout(small_modbus_t *smb,int timeout_ms)
{
    smb->timeout_byte = timeout_ms;
    return MODBUS_OK;
}

int modbus_set_slave(small_modbus_t *smb, int slave)
{
    if(smb!=NULL)
    {
        if((slave > 0)&&(slave < 247))
        {
            smb->slave_addr = slave;
            return MODBUS_OK;
        }
    }
    return MODBUS_OK;
}

int modbus_set_debug(small_modbus_t *smb, int level)
{
    if(smb!=NULL)
    {
       smb->debug_level = level;
    }
    return MODBUS_OK;
}

/* master start request */
int modbus_start_request(small_modbus_t *smb,uint8_t *request,int function,int addr,int num,void *write_data)
{
	int len = 0;
	uint16_t count = 0;
	
	if(modbus_check_addr_num(function,addr,num)) //检查参数
	{
		//组建modbus帧头
		len = smb->core->build_request_header(smb,request,smb->slave_addr,function,addr,num);
		//填充数据
		switch(function)
		{
			case MODBUS_FC_WRITE_SINGLE_COIL:
			{
				request[len-2] = (*((uint8_t*)write_data))?0xFF:0x00; //主机转modbus
				request[len-1] = 0x00;
			}break;
			case MODBUS_FC_WRITE_SINGLE_REGISTER:
			{
				modbus_reg_h2m(&(request[len-2]),write_data,1); //主机转modbus
			}break;
			case MODBUS_FC_WRITE_MULTIPLE_COILS:
			{
				//计算返回的字节数
				count = (num / 8) + ((num % 8) ? 1 : 0);
				request[len++] = count;
				
				modbus_coil_h2m(&(request[len]),write_data,num); //主机转modbus
				
				len += count; //添加长度
			}break;
			case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
			{
				//计算返回的字节数
				count = (num * 2);
				request[len++] = count;
				
				modbus_reg_h2m(&(request[len]),write_data,num); //主机转modbus
				
				len += count; //添加长度
			}break;
		}
		//发送
		len = smb->core->check_send_pre(smb,request,len);
		if(len > 0)
		{
			 modbus_write(smb,request, len);
			 return len;
		}
	}
	return MODBUS_FAIL_REQUEST;
}
/* master wait for confirmation message */
int modbus_wait_confirm(small_modbus_t *smb,uint8_t *response)
{
    int rc = 0;
    int wait_time = 0;
    int read_want = 0;
    int read_length = 0;
    int read_position = 0;
    int function = 0;

    wait_time = smb->timeout_frame;
    read_want = smb->core->len_header + 1;  //header + function code
	
    while (read_want != 0)
    {
				rc = modbus_want_read(smb,response + read_length,read_want,wait_time);
        if(rc <= MODBUS_OK)
        {
						if(rc < MODBUS_TIMEOUT)
						{
							modbus_debug_error(smb,"[%d]read(%d) error\n",rc,read_want);
						}
            return rc;
        }
        if(rc != read_want)
        {
            modbus_debug_info(smb,"[%d]read(%d) less\n",rc,read_want);
        }
				
        read_length += rc;  //sum byte length
        read_want -= rc;    //sub byte length

        if(read_want == 0)//read ok
        {
            if(read_position==0)/* Function code position */
            {
                function = response[smb->core->len_header];
                switch (function)
                {
                case MODBUS_FC_READ_HOLDING_COILS:
                case MODBUS_FC_READ_INPUTS_COILS:
                case MODBUS_FC_READ_HOLDING_REGISTERS:
                case MODBUS_FC_READ_INPUT_REGISTERS:
                    read_want = 1; //read data length(1)
                    break;
                case MODBUS_FC_WRITE_SINGLE_COIL:
                case MODBUS_FC_WRITE_SINGLE_REGISTER:
                case MODBUS_FC_WRITE_MULTIPLE_COILS:
                case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                    read_want = 4;  //write addr(2)+num(2)
                    break;
                case MODBUS_FC_MASK_WRITE_REGISTER:
                    read_want = 6;  //data length
                    break;
                default:
                    read_want = 1;  //read data length(1)
                }
                read_position = 1;
            }
            else if(read_position==1)/* Data */
            {
                function = response[smb->core->len_header];
                switch(function)
                {
                case MODBUS_FC_READ_HOLDING_COILS:
                case MODBUS_FC_READ_INPUTS_COILS:
                case MODBUS_FC_READ_HOLDING_REGISTERS:
                case MODBUS_FC_READ_INPUT_REGISTERS:
                    read_want = response[smb->core->len_header+1];
                    break;
                default:
                    read_want = 0;
                }
                read_want += smb->core->len_checksum;
                if((read_want+read_length)> smb->core->len_adu_max )
                {
                    modbus_debug_error(smb,"More than ADU %d > %d\n",(read_want+read_length),smb->core->len_adu_max);
                    return MODBUS_FAIL;
                }
                read_position = 2;
            }
        }
				if(read_want)
				{
					wait_time = smb->timeout_byte * read_want; // byte_time * byte_num
				}
    }
    return smb->core->check_wait_response(smb,response,read_length);
}
/* master handle confirmation message */
int modbus_handle_confirm(small_modbus_t *smb,uint8_t *request,uint16_t request_len,uint8_t *response,uint16_t response_len,void *read_data)
{
    uint8_t request_function = request[smb->core->len_header];
    uint8_t response_function = response[smb->core->len_header];
    uint16_t calc_length = smb->core->len_header + smb->core->len_checksum ; // header + checksum
    uint16_t rw_num = 0;
    uint16_t byte_num = 0;
    uint16_t temp = 0;
    if(response_function >= 0x80)
    {
        if((response_function - 0x80) == request_function)
        {
            modbus_debug_error(smb,"request function code %d\n",request_function);
        }
        modbus_debug_error(smb,"response exception code %d\n",-response_function);
        return -response_function;
    }
    if(request_function == response_function)
    {
        //计算帧长度
        switch (request_function)
        {
            case MODBUS_FC_READ_HOLDING_COILS:
            case MODBUS_FC_READ_INPUTS_COILS:
                {
                    temp = (request[smb->core->len_header+3]<<8)|(request[smb->core->len_header+4]);  //data length
                    calc_length += (2 + (temp / 8) + ((temp % 8) ? 1 : 0));
                }
                break;
            case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            case MODBUS_FC_READ_HOLDING_REGISTERS:
            case MODBUS_FC_READ_INPUT_REGISTERS:
                {
                    temp = (request[smb->core->len_header+3]<<8)|(request[smb->core->len_header+4]);  //data length
                    calc_length += (2 + 2*temp);
                }
                break;
            case MODBUS_FC_READ_EXCEPTION_STATUS:
                calc_length += 3;
                break;
            case MODBUS_FC_MASK_WRITE_REGISTER:
                calc_length += 7;
                break;
            default:
                calc_length += 5;
        }
				//比较帧长度是否一致
        if(calc_length == response_len)
        {
            //read data
            switch (response_function)
            {
            case MODBUS_FC_READ_HOLDING_COILS:
            case MODBUS_FC_READ_INPUTS_COILS:
                {
									// 请求线圈数量
									rw_num = (request[smb->core->len_header+3]<<8)|(request[smb->core->len_header+4]);
									// 请求线圈字节数
									temp = ((rw_num / 8) + ((rw_num % 8) ? 1 : 0));
									// 返回线圈字节数
									byte_num = (response[smb->core->len_header+1]);
									if((uint8_t)temp == byte_num)
									{
										//modbus转主机
										modbus_coil_m2h(read_data,&(response[smb->core->len_header+2]),rw_num);
										return MODBUS_OK;
									}
                }
                break;
            case MODBUS_FC_READ_HOLDING_REGISTERS:
            case MODBUS_FC_READ_INPUT_REGISTERS:
                {
									// 计算读取线圈数量
									rw_num = (request[smb->core->len_header+3]<<8)|(request[smb->core->len_header+4]);  //data length
									temp = rw_num*2;
									byte_num = (response[smb->core->len_header+1]);
									if((uint8_t)temp == byte_num)
									{
										//modbus转主机
										modbus_reg_m2h(read_data,&(response[smb->core->len_header+2]),rw_num);
										return MODBUS_OK;
									}
                }
                break;
            case MODBUS_FC_WRITE_MULTIPLE_COILS:
            case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
                {
									rw_num = (request[smb->core->len_header+3]<<8)|(request[smb->core->len_header+4]);  //data length
									temp = (response[smb->core->len_header+3]<<8)|(response[smb->core->len_header+4]);  //data length
									if(rw_num == temp)
									{
										//回复写成功
										return MODBUS_OK;
									}
                }
                break;
            case MODBUS_FC_MASK_WRITE_REGISTER:
            case MODBUS_FC_WRITE_AND_READ_REGISTERS:
            case MODBUS_FC_READ_EXCEPTION_STATUS:
            default:
                {
									return MODBUS_OK;
                }
            }
        }
    }
    return MODBUS_FAIL_CONFIRM;
}

/* master read */
int modbus_read_bits(small_modbus_t *smb, int addr, int num,uint8_t *read_data)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;

    request_len = modbus_start_request(smb,request,MODBUS_FC_READ_HOLDING_COILS,addr,num,NULL);
    if(request_len < 0)
    {
			return request_len;
		}
		response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, read_data);
}

/* master read */
int modbus_read_input_bits(small_modbus_t *smb, int addr, int num,uint8_t *read_data)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;

    request_len = modbus_start_request(smb,request,MODBUS_FC_READ_INPUTS_COILS,addr,num,NULL);
    if(request_len < 0)
    {
			return request_len;
		}
    response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, read_data);
}
/* master read */
int modbus_read_registers(small_modbus_t *smb, int addr, int num,uint16_t *read_data)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;

    request_len = modbus_start_request(smb,request,MODBUS_FC_READ_HOLDING_REGISTERS,addr,num,NULL);
    if(request_len < 0)
    {
			return request_len;
		}
		response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, read_data);
}
/* master read */
int modbus_read_input_registers(small_modbus_t *smb, int addr, int num,uint16_t *read_data)
{
	int request_len = 0;
	int response_len = 0;
	uint8_t *request = smb->write_buff;
	uint8_t *response = smb->read_buff;


	request_len = modbus_start_request(smb,request,MODBUS_FC_READ_INPUT_REGISTERS,addr,num,NULL);
	if(request_len < 0)
	{
		return request_len;
	}
	response_len = modbus_wait_confirm(smb, response);
	if(response_len <= 0)
	{
		return response_len;
	}
	return modbus_handle_confirm(smb, request,request_len, response,response_len, read_data);
}
/* master write */
int modbus_write_bit(small_modbus_t *smb, int addr,int write_status)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;
    int status = write_status?0xFF00:0x0;

    request_len = modbus_start_request(smb,request,MODBUS_FC_WRITE_SINGLE_COIL,addr,1,&status);
    if(request_len < 0)
    {
			return request_len;
		}
		response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, NULL);
}
/* master write */
int modbus_write_register(small_modbus_t *smb, int addr,int write_value)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;
    int value = write_value;

    request_len = modbus_start_request(smb,request,MODBUS_FC_WRITE_SINGLE_REGISTER,addr,1,&value);
    if(request_len < 0)
    {
			return request_len;
		}
		response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, NULL);
}
/* master write */
int modbus_write_bits(small_modbus_t *smb, int addr, int num,uint8_t *write_data)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;

    request_len = modbus_start_request(smb,request,MODBUS_FC_WRITE_MULTIPLE_COILS,addr,num,write_data);
    if(request_len < 0)
    {
			return request_len;
		}
		response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, NULL);
}
/* master write */
int modbus_write_registers(small_modbus_t *smb, int addr, int num,uint16_t *write_data)
{
    int request_len = 0;
    int response_len = 0;
    uint8_t *request = smb->write_buff;
    uint8_t *response = smb->read_buff;

    request_len = modbus_start_request(smb,request,MODBUS_FC_WRITE_MULTIPLE_REGISTERS,addr,num,write_data);
    if(request_len < 0)
    {
			return request_len;
		}
		response_len = modbus_wait_confirm(smb, response);
		if(response_len <= 0)
		{
			return response_len;
		}
		return modbus_handle_confirm(smb, request,request_len, response,response_len, NULL);
}
/* master write and read */
int modbus_mask_write_register(small_modbus_t *smb, int addr, uint16_t and_mask, uint16_t or_mask)
{
    return MODBUS_FAIL;
}
int modbus_write_and_read_registers(small_modbus_t *smb, int write_addr, int write_nb,uint16_t *src, int read_addr, int read_nb,uint16_t *dest)
{
    return MODBUS_FAIL;
}






/* slave wait query data */
int modbus_slave_wait(small_modbus_t *smb,uint8_t *request,int32_t wait_time)
{
    int rc = 0;
		int read_step = 0;
    int read_want = 0;
    int read_length = 0;
    int function = 0;

    read_want = smb->core->len_header + 1;  //header + function code
	
    while (read_want != 0)
    {
				rc = modbus_want_read(smb,request + read_length,read_want,wait_time);
        if(rc <= MODBUS_OK)
        {
						if(rc < MODBUS_TIMEOUT)
						{
							modbus_debug_error(smb,"[%d]read(%d) error\n",rc,read_want);
						}
            return rc;
        }
        if(rc != read_want)
        {
            modbus_debug_info(smb,"[%d]read(%d) less\n",rc,read_want);
        }

        read_length += rc;  //sum byte length
        read_want -= rc;    //sub byte length

        if(read_want == 0)//read ok
        {
					switch(read_step)
					{
						case 0:/* Function code position */
						{
							function = request[smb->core->len_header];
							if(function <= MODBUS_FC_WRITE_SINGLE_REGISTER)
							{
									read_want = 4;
							}else if((function == MODBUS_FC_WRITE_MULTIPLE_COILS) ||
									 (function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS) )
							{
									read_want = 5;
							}else if(function == MODBUS_FC_MASK_WRITE_REGISTER)
							{
									read_want = 6;
							}else if(function == MODBUS_FC_WRITE_AND_READ_REGISTERS)
							{
									read_want = 9;
							}else
							{
									read_want = 0; //not want read
							}
							if(read_want!=0)
							{
								read_step = 1;
								break;
							}
						}
						case 1:
						{
							function = request[smb->core->len_header];
							if((function == MODBUS_FC_WRITE_MULTIPLE_COILS)||
								(function == MODBUS_FC_WRITE_MULTIPLE_REGISTERS))
							{
								read_want = request[smb->core->len_header + 5];
							}else if(function == MODBUS_FC_WRITE_AND_READ_REGISTERS)
							{
								read_want = request[smb->core->len_header + 9];
							}else
							{
								read_want = 0;
							}
							read_want += smb->core->len_checksum;
							read_step = 2;
							if((read_want+read_length)> smb->core->len_adu_max )
							{
									modbus_debug_error(smb,"More than ADU %d > %d\n",(read_want+read_length),smb->core->len_adu_max);
									return MODBUS_FAIL;
							}
						}break;
					}
        }
				if(read_want)
				{
					wait_time = smb->timeout_byte * read_want; // byte_time * byte_num
				}
    }
    return smb->core->check_wait_request(smb,request,read_length);
}

/* slave handle query data for callback */
int modbus_slave_handle(small_modbus_t *smb,uint8_t *request,uint16_t request_len,small_modbus_slave_callback_t slave_callback)
{
	//uint8_t *request = smb->read_buff;
	uint16_t response_len = 0;
	uint8_t *response = smb->write_buff;
	uint8_t query_slave = smb->slave_addr;//request[smb->core->len_header-1];
	uint8_t query_function = request[smb->core->len_header];
	uint16_t query_address = 0;
	uint16_t query_num = 0;
	
	int bytes = MODBUS_OK;
	int response_exception = MODBUS_OK;

	/* Data are flushed on illegal number of values errors. */
	switch (query_function)
	{
		case MODBUS_FC_READ_HOLDING_COILS:
		case MODBUS_FC_READ_INPUTS_COILS:
		{
			query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];  //请求地址
			query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];  //请求  数量
			 //检查查询地址数量
			if(modbus_check_addr_num(query_function,query_address,query_num))
			{
				 //准备应答数据头,计算数据长度
				response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
				 //回调,返回的是读取到的线圈数量
				if(slave_callback)
						response_exception = slave_callback(smb,query_function,query_address,query_num, (response+response_len+1) );
				 //检查返回地址数量
				if(modbus_check_addr_num(query_function,query_address,response_exception))
				{ 
					//计算数据使用字节数
					bytes = (response_exception / 8) + ((response_exception % 8) ? 1 : 0);
					response[response_len] = bytes;
					//主机转modbus
					modbus_coil_h2m((response+response_len+1),(response+response_len+1),response_exception);
					//返回长度
					response_len +=  (bytes+1);
				}else
				{
					modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d rc:%d\n",query_slave,query_function,query_address,query_num,response_exception);
					response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
				}
			}else
			{
				modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d not at[1-%d]\n",query_slave,query_function,query_address,query_num,MODBUS_MAX_READ_BITS);
				response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
		}break;
		case MODBUS_FC_READ_HOLDING_REGISTERS:
		case MODBUS_FC_READ_INPUT_REGISTERS:
		{
			query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];  //请求地址
			query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];  //请求  数量
			 //检查查询地址数量
			if(modbus_check_addr_num(query_function,query_address,query_num))
			{
				//准备应答数据头,计算数据长度
				response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
				//回调,返回的是读取到的寄存器数量
				if(slave_callback)
				{
					response_exception = slave_callback(smb,query_function,query_address,query_num, (response+response_len+1) ); 
				}
				//检查返回地址数量
				if(modbus_check_addr_num(query_function,query_address,response_exception)) 
				{
					//计算数据使用字节数
					bytes = response_exception*2;
					response[response_len] = bytes;
					//主机转modbus
					modbus_reg_h2m((response+response_len+1),(response+response_len+1),response_exception);
					//返回长度
					response_len +=  (bytes+1);
				}else
				{
					modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d rc:%d\n",query_slave,query_function,query_address,query_num,response_exception);
					response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
				}
			}else
			{
				modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d not at[1-%d]\n",query_slave,query_function,query_address,query_num,MODBUS_MAX_READ_REGISTERS);
				response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
		}break;
		case MODBUS_FC_WRITE_SINGLE_COIL:
		{
			query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
			query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];  //请求 值
			//准备应答数据头,计算数据长度
			response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
			 //回调,返回的是读取到的线圈数量
			if(slave_callback)
			{
				//modbus转主机
				modbus_coil_m2h((request+smb->core->len_header + 3),(request+smb->core->len_header + 3),1);
				//回调
				response_exception = slave_callback(smb,query_function,query_address,1,(request+smb->core->len_header + 3));
			}
			if(response_exception == 1)  //只能是1
			{
				response[response_len++] = (query_address>>8);
				response[response_len++] = (query_address&0x00ff);
				response[response_len++] = (query_num>>8); //请求 值
				response[response_len++] = (query_num&0x00ff);
			}
			else
			{
				modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,value:%d rc:%d\n",query_slave,query_function,query_address,query_num,response_exception);
				response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
		}break;
		case MODBUS_FC_WRITE_SINGLE_REGISTER:
		{
			query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
			query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];  //请求 值
			//准备应答数据头,计算数据长度
			response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
			 //回调,返回的是读取到的寄存器数量
			if(slave_callback)
			{
				//modbus转主机
				modbus_reg_m2h((request+smb->core->len_header + 3),(request+smb->core->len_header + 3),1);
				//回调
				response_exception = slave_callback(smb,query_function,query_address,1,(request+smb->core->len_header + 3));
			}
			if(response_exception == 1)  //只能是1
			{
				response[response_len++] = (query_address>>8);
				response[response_len++] = (query_address&0x00ff);
				response[response_len++] = (query_num>>8); //请求 值
				response[response_len++] = (query_num&0x00ff);
			}
			else
			{
				modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,value:%d rc:%d\n",query_slave,query_function,query_address,query_num,response_exception);
				response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
		}break;
		case MODBUS_FC_WRITE_MULTIPLE_COILS:
		{
			query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
			query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];  //请求 数量
			//检查查询地址数量
			if(modbus_check_addr_num(query_function,query_address,query_num))
			{
				//准备应答数据头,计算数据长度
				response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
				//回调,返回的是读取到的寄存器数量
				if(slave_callback)
				{
					//modbus转主机
					modbus_coil_m2h((request+smb->core->len_header + 6),(request+smb->core->len_header + 6),query_num);
					//回调
					response_exception = slave_callback(smb,query_function,query_address,query_num, (request+smb->core->len_header + 6) );
				}
				//检查返回地址数量
				if(modbus_check_addr_num(query_function,query_address,response_exception))
				{
					response[response_len++] = (query_address>>8);
					response[response_len++] = (query_address&0x00ff);
					response[response_len++] = (query_num>>8);
					response[response_len++] = (query_num&0x00ff);
				}else
				{
					modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d rc:%d\n",query_slave,query_function,query_address,query_num,response_exception);
					response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
				}
			}else
			{
				modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d not at[1-%d]\n",query_slave,query_function,query_address,query_num,MODBUS_MAX_WRITE_BITS);
				response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
		}break;
		case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
		{
			query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
			query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];  //请求 数量
			//检查查询地址数量
			if(modbus_check_addr_num(query_function,query_address,query_num))
			{
				//准备应答数据头,计算数据长度
				response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
				//回调,返回的是读取到的寄存器数量
				if(slave_callback)
				{
					//modbus转主机
					modbus_reg_m2h((request+smb->core->len_header + 6),(request+smb->core->len_header + 6),query_num);
					//回调
					response_exception = slave_callback(smb,query_function,query_address,query_num, (request+smb->core->len_header + 6) );
				}
				//检查返回地址数量
				if(modbus_check_addr_num(query_function,query_address,response_exception))
				{
					response[response_len++] = (query_address>>8);
					response[response_len++] = (query_address&0x00ff);
					response[response_len++] = (query_num>>8);
					response[response_len++] = (query_num&0x00ff);
				}
				else
				{
					modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d rc:%d\n",query_slave,query_function,query_address,query_num,response_exception);
					response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
				}
			}else
			{
				modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,addr:%d,num:%d not at[1-%d]\n",query_slave,query_function,query_address,query_num,MODBUS_MAX_WRITE_REGISTERS);
				response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
			}
		}break;
		case MODBUS_FC_REPORT_SLAVE_ID:
		{
			//准备应答数据头,计算数据长度
			response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
			response[response_len++] = 9;
			response[response_len++] = smb->slave_addr;//_REPORT_SLAVE_ID;
			response[response_len++] = 0xFF;
			response[response_len++] = 'S';
			response[response_len++] = 'M';
			response[response_len++] = 'O';
			response[response_len++] = 'D';
			response[response_len++] = 'B';
			response[response_len++] = 'U';
			response[response_len++] = 'S';
		}break;
		case MODBUS_FC_READ_EXCEPTION_STATUS:
		case MODBUS_FC_MASK_WRITE_REGISTER:
		case MODBUS_FC_WRITE_AND_READ_REGISTERS:
		{
			modbus_debug_error(smb,"slave:0x%0X,Unknown Modbus function code: 0x%0X\n",query_slave,query_function);
			response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
		}break;
		default:
		{
			modbus_debug_error(smb,"slave:0x%0X,Unknown Modbus function code: 0x%0X\n",query_slave,query_function);
			response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
		}break;
	}
	if(response_exception <= MODBUS_EXCEPTION)
	{
		response_len = smb->core->build_response_header(smb,response,query_slave,query_function+0x80);
		response[response_len++] = response_exception;
	}
	if(response_len)
	{
		response_len = smb->core->check_send_pre(smb,response,response_len);
		if(response_len > 0)
		{
			 modbus_write(smb,response, response_len);
			 return response_len;
		}
	}
	return MODBUS_FAIL_HANDLE;
}

/* slave wait and handle query for callback */
int modbus_slave_wait_handle(small_modbus_t *smb,small_modbus_slave_callback_t slave_callback,int32_t waittime)
{
	int rc = 0;
	//uint8_t *confirm = smb->write_buff;
	uint8_t *request = smb->read_buff;
	rc = modbus_slave_wait(smb,request,waittime);
	if(rc < 0)
	{
		return rc;
	}
	return modbus_slave_handle(smb,request,rc,slave_callback);
}

#ifdef MODBUS_SLAVE_MAPPING_TABLE
/* slave handle query data for mapping */
int modbus_slave_handle_mapping(small_modbus_t *smb,uint8_t *request,uint16_t request_len,small_modbus_slave_mapping_t * slave_mapping_tab)
{
	//uint8_t *request = smb->read_buff;
	int response_exception = MODBUS_OK;
	uint16_t response_len = 0;
	uint8_t *response = smb->write_buff;
	uint8_t query_slave = smb->slave_addr;//request[smb->core->len_header-1];
	uint8_t query_function = request[smb->core->len_header];
	uint16_t query_address = 0;
	uint16_t query_num = 0;
	uint16_t map_address = 0;
	uint16_t map_num = 0;
	uint8_t *map_8array;
	uint16_t *map_16array;
	uint16_t temp = 0;

	/* Data are flushed on illegal number of values errors. */
	switch (query_function)
	{
			case MODBUS_FC_READ_HOLDING_COILS:
			case MODBUS_FC_READ_INPUTS_COILS:
			{
					if(query_function == MODBUS_FC_READ_HOLDING_COILS)
					{
							map_address = slave_mapping_tab->bit.start;
							map_num = slave_mapping_tab->bit.num;
							map_8array = slave_mapping_tab->bit.array;
					}else
					{
							map_address = slave_mapping_tab->input_bit.start;
							map_num = slave_mapping_tab->input_bit.num;
							map_8array = slave_mapping_tab->input_bit.array;
					}
					query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
					if((map_address <= query_address) && (query_address <= (map_address + map_num)) ) //起始地址
					{
							int diff = query_address - map_address;
							query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];//查询数量
							if((0  < query_num )&&( query_num <= (map_num-diff)))
							{
									response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
									temp = modbus_array2bit(&(response[response_len]), map_8array+diff, query_num);  //数组转modbus bit
									response_len += temp;
							}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;}
					}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;}
			}break;

			case MODBUS_FC_READ_HOLDING_REGISTERS:
			case MODBUS_FC_READ_INPUT_REGISTERS:
			{
					if(query_function == MODBUS_FC_READ_HOLDING_REGISTERS)
					{
							map_address = slave_mapping_tab->registers.start;
							map_num = slave_mapping_tab->registers.num;
							map_16array = slave_mapping_tab->registers.array;
					}else
					{
							map_address = slave_mapping_tab->input_registers.start;
							map_num = slave_mapping_tab->input_registers.num;
							map_16array = slave_mapping_tab->input_registers.array;
					}
					query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
					if((map_address <= query_address) && (query_address <= (map_address + map_num)) ) //起始地址
					{
							int diff = query_address - map_address;
							query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];//查询数量
							if((0  < query_num )&&( query_num <= (map_num-diff)))
							{
									response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
									temp = modbus_array2reg(&(response[response_len]), map_16array+diff, query_num);  //数组转modbus reg
									response_len += temp;
							}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;}
					}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;}
			}break;

			case MODBUS_FC_WRITE_SINGLE_COIL:
			{
					map_address =   slave_mapping_tab->bit.start;
					map_num =       slave_mapping_tab->bit.num;
					map_8array =    slave_mapping_tab->bit.array;
					query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
					if((map_address <= query_address) && (query_address <= (map_address + map_num)) ) //起始地址
					{
							int diff = query_address - map_address;
							query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];//数据
							if (query_num == 0xFF00 || query_num == 0x0)
							{
									map_8array[diff] = query_num ? 1 : 0;
									response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
									response[response_len++] = (query_address>>8);
									response[response_len++] = (query_address&0x00ff);
									response[response_len++] = (query_num>>8);
									response[response_len++] = (query_num&0x00ff);
							}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;}
					}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;}
			}break;
			case MODBUS_FC_WRITE_SINGLE_REGISTER:
			{
					map_address =   slave_mapping_tab->registers.start;
					map_num =       slave_mapping_tab->registers.num;
					map_16array =    slave_mapping_tab->registers.array;
					query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
					if((map_address <= query_address) && (query_address <= (map_address + map_num)) ) //起始地址
					{
							int diff = query_address - map_address;
							query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];//数据
							map_16array[diff] = query_num;
							response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
							response[response_len++] = (query_address>>8);
							response[response_len++] = (query_address&0x00ff);
							response[response_len++] = (query_num>>8);
							response[response_len++] = (query_num&0x00ff);
					}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;}
			}break;
			case MODBUS_FC_WRITE_MULTIPLE_COILS:
			{
					map_address =   slave_mapping_tab->bit.start;
					map_num =       slave_mapping_tab->bit.num;
					map_8array =    slave_mapping_tab->bit.array;
					query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
					if((map_address <= query_address) && (query_address <= (map_address + map_num)) ) //起始地址
					{
							int diff = query_address - map_address;
							query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];//查询数量
							temp = (query_num / 8) + ((query_num % 8) ? 1 : 0); //数据长度
							if((0  < query_num )&&( query_num <= (map_num-diff)) && (temp == request[smb->core->len_header + 5]) )
							{
									temp = modbus_bit2array(map_8array+diff,&(response[smb->core->len_header + 5]),query_num);   //modbus bit 转数组
									response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
									response[response_len++] = (query_address>>8);
									response[response_len++] = (query_address&0x00ff);
									response[response_len++] = (query_num>>8);
									response[response_len++] = (query_num&0x00ff);
							}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;}
					}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;}
			}break;
			case MODBUS_FC_WRITE_MULTIPLE_REGISTERS:
			{
					map_address =   slave_mapping_tab->registers.start;
					map_num =       slave_mapping_tab->registers.num;
					map_16array =    slave_mapping_tab->registers.array;
					query_address = (request[smb->core->len_header + 1] << 8) + request[smb->core->len_header + 2];
					if((map_address <= query_address) && (query_address <= (map_address + map_num)) ) //起始地址
					{
							int diff = query_address - map_address;
							query_num = (request[smb->core->len_header + 3] << 8) + request[smb->core->len_header + 4];//查询数量
							temp = (query_num * 2); //数据长度
							if((0  < query_num )&&( query_num <= (map_num-diff)) && (temp == request[smb->core->len_header + 5]) )
							{
									temp = modbus_reg2array(map_16array+diff,&(response[smb->core->len_header + 5]),query_num);   //modbus bit 转数组
									response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
									response[response_len++] = (query_address>>8);
									response[response_len++] = (query_address&0x00ff);
									response[response_len++] = (query_num>>8);
									response[response_len++] = (query_num&0x00ff);
							}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;}
					}else{response_exception = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;}
			}break;
			case MODBUS_FC_REPORT_SLAVE_ID:
			{
					response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
//            temp = strlen(_modbus_ver);
//            response_len = smb->core->build_response_header(smb,response,query_slave,query_function);
//            response[response_len++] =  temp+2;
//            response[response_len++] =  smb->slave_addr;
//            /* Run indicator status to ON */
//            response[response_len++] =  0XFF;
//            memcpy(response + response_len,_modbus_ver, temp);
//            response_len += temp;
			}break;
			case MODBUS_FC_READ_EXCEPTION_STATUS:
			{
					response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
			}break;
			case MODBUS_FC_MASK_WRITE_REGISTER:
			{
					response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
			}break;
			case MODBUS_FC_WRITE_AND_READ_REGISTERS:
			{
					response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
			}break;
			default:
			{
					response_exception = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
			}break;
	}

	if(response_exception <= MODBUS_EXCEPTION)
	{
			switch(response_exception)
			{
					case MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE:
					{
							modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,Illegal num:%d not at [1-%d]\n",query_slave,query_function,query_num,map_num);
					}break;
					case MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS:
					{
							modbus_debug_error(smb,"slave:0x%0X,function:0x%0X,Illegal data:%d not at [1-%d]\n",query_slave,query_function,query_address,map_address);
					}break;
					default:
					{
							modbus_debug_error(smb,"slave:0x%0X,Unknown Modbus function code: 0x%0X\n",query_slave,query_function);
					 }break;
			}
			response_len = smb->core->build_response_header(smb,response,query_slave,query_function+0x80);
			response[response_len++] = response_exception;
	}

	if(response_len)
	{
			response_len = smb->core->check_send_pre(smb,response,response_len);
			if(response_len > 0)
			{
					 modbus_write(smb,response, response_len);
					 return response_len;
			}
	}
	return MODBUS_FAIL_HANDLE;
}
/* slave wait and handle query for mapping */
int modbus_slave_wait_handle_mapping(small_modbus_t *smb,small_modbus_slave_mapping_t * slave_mapping_tab,int32_t waittime)
{
	int rc = 0;
	//uint8_t *confirm = smb->write_buff;
	uint8_t *request = smb->read_buff;
	rc = modbus_slave_wait(smb,request,waittime);
	if(rc < 0)
	{
		return rc;
	}
	return modbus_slave_handle_mapping(smb,request,rc,slave_mapping_tab);
}
#endif

