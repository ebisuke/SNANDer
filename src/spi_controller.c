/*======================================================================================
 * MODULE NAME: spi
 * FILE NAME: spi_controller.c
 *
 * FUNCTIONS
 *
 *      SPI_CONTROLLER_Enable_Manual_Mode To provide interface for Enable SPI Controller Manual Mode.
 *      SPI_CONTROLLER_Write_One_Byte     To provide interface for write one byte to SPI bus.
 *      SPI_CONTROLLER_Write_NByte        To provide interface for write N bytes to SPI bus.
 *      SPI_CONTROLLER_Read_NByte         To provide interface for read N bytes from SPI bus.
 *      SPI_CONTROLLER_Chip_Select_Low    To provide interface for set chip select low in SPI bus.
 *      SPI_CONTROLLER_Chip_Select_High   To provide interface for set chip select high in SPI bus.
 *
 * DEPENDENCIES
 *
 * * $History: $
 * MODIFICTION HISTORY:
 *
 *======================================================================================
 */
#include "ch341a_spi.h"
#include "spi_controller.h"

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Enable_Manual_Mode( void )
{
	return 0;
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Write_One_Byte( u8  data )
{
	return (SPI_CONTROLLER_RTN_T)spi_controller->send_command(1, 0, &data, NULL);
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Chip_Select_High( void )
{
	if(spi_controller->cs_release)
		spi_controller->cs_release();
	return 0;
	//return (SPI_CONTROLLER_RTN_T)enable_pins(false);
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Chip_Select_Low( void )
{
	if(spi_controller->cs_assert)
		spi_controller->cs_assert();
	return 0;
	//return (SPI_CONTROLLER_RTN_T)enable_pins(true);
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Read_NByte( u8 *ptr_rtn_data, u32 len, SPI_CONTROLLER_SPEED_T speed )
{
	u32 chunk_sz = min(len, spi_controller->max_transfer);
	int ret;

	/*
	 * Handle chunking the transfer when the controller has a smaller max_transfer than the
	 * requested amount.
	 */
	while(len) {
		int read_sz = min(chunk_sz, len);
		ret = spi_controller->send_command(0, read_sz, NULL, ptr_rtn_data);
		ptr_rtn_data += read_sz;
		len -= read_sz;
		if(ret)
			break;
	}

	return (SPI_CONTROLLER_RTN_T) ret;
}

SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Write_NByte( u8 *ptr_data, u32 len, SPI_CONTROLLER_SPEED_T speed )
{
	u32 chunk_sz = min(len, spi_controller->max_transfer);
	int ret;

	/*
	 * Handle chunking the transfer when the controller has a smaller max_transfer than the
	 * requested amount.
	 */
	while(len) {
		int write_sz = min(chunk_sz, len);
		ret = spi_controller->send_command(write_sz, 0, ptr_data, NULL);
		ptr_data += write_sz;
		len -= write_sz;
		if(ret)
			break;
	}

	return (SPI_CONTROLLER_RTN_T) ret;
}

#if 0
SPI_CONTROLLER_RTN_T SPI_CONTROLLER_Xfer_NByte( u8 *ptr_data_in, u32 len_in, u8 *ptr_data_out, u32 len_out, SPI_CONTROLLER_SPEED_T speed )
{
	return (SPI_CONTROLLER_RTN_T)spi_controller->send_command(len_out, len_in, ptr_data_out, ptr_data_in);
}
#endif
/* End of [spi_controller.c] package */
