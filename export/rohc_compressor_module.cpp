/*
 * Copyright 2013,2014 Didier Barvaux
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Modified by Jeferson Santiago da Silva and Laurent Olivier Chiquette
 *
 */

/* includes required to use the compression part of the ROHC library */
#include "rohc_compressor_module.h"

namespace ROHC {

// Create ROHC compressor
int RohcCompressorEntity::compress_init(bool debug_enable)
{
	/* Create a ROHC compressor to operate:
	 *  - with large CIDs,
	 *  - with the maximum of 5 streams (MAX_CID = 4),
	 *  - in Unidirectional mode (U-mode).
	 */

  comp_debug_enable = debug_enable;
	if (comp_debug_enable) printf("\ncreate the ROHC compressor\n");

	comp_state = rohc_comp_new2(ROHC_LARGE_CID, 4, gen_false_random_num, NULL);

	if(comp_state == NULL)
	{
	  if (comp_debug_enable) printf("\nfailed create the ROHC compressor\n");
	  goto error;
	}

  if (!rohc_comp_set_wlsb_window_width(comp_state, 4)){
	  if (comp_debug_enable) printf("\nfailed create the ROHC W-LSB window\n");
  }

	/* Enable Debug trace */
	if (comp_debug_enable){ 
    if (!rohc_comp_set_traces_cb2(comp_state, print_rohc_traces, NULL)) {
	    if (comp_debug_enable) printf("\nfailed to enable traces\n");
	  	goto error;	  
    }
  }
  
  if(!rohc_comp_set_rtp_detection_cb(comp_state, rohc_comp_rtp_cb, NULL)){
	    if (comp_debug_enable) printf("\nfailed to set the callback RTP detection\n");
	  	goto error;	  
  }

	/* Enable the compression profiles you need */
	for (int i = 0; i <= ROHC_PROFILE_IP; ++i) {
		const char *profile_name = rohc_get_profile_descr((rohc_profile_t)i); 	
	  if (comp_debug_enable) printf("\nEnable %s ROHC compression profile\n", profile_name);
		if(!rohc_comp_enable_profile(comp_state, (rohc_profile_t)i))
		{
			if (comp_debug_enable) printf("\nfailed to enable the %s profile\n", profile_name);
			rohc_comp_free(comp_state);
			goto error;
		}
	}
	
	if (comp_debug_enable) printf("\nCompressor initialization ended successfully.\n");
	return 0;

error:
	if (comp_debug_enable) printf("\nan error occured during program execution, abort program\n");
	return 1;
}

/**
 * @brief The main entry point for the program
 * @return      0 in case of success, 1 otherwise
*/
int RohcCompressorEntity::compress_header(unsigned char *compressed_header_buffer, unsigned char *uncompressed_header_buffer,
						size_t *comp_header_size, size_t uncomp_header_size)
{
	// Define IP and ROHC packets
	/* the buffer that will contain the ROHC packet to compress */
	struct rohc_buf rohc_packet = rohc_buf_init_empty(compressed_header_buffer, BUFFER_SIZE);

	/* the buffer that will contain the resulting IP packet */
	struct rohc_buf ip_packet = rohc_buf_init_empty(uncompressed_header_buffer, BUFFER_SIZE);

	rohc_status_t status;

	/* Build an ip packet from uncompressed headers */
	if (comp_debug_enable) printf("\nbuild a IP packet\n");
	for (size_t i = 0; i < uncomp_header_size; ++i) {
    rohc_buf_byte_at(ip_packet, i) = uncompressed_header_buffer[i];
	}
	ip_packet.len = uncomp_header_size;

	/* dump the newly-created ip packet on terminal */
	dump_packet(ip_packet);

	/* Compress this IP packet */
	if (comp_debug_enable) printf("\ncompress the IP packet\n");
	status = rohc_compress4(comp_state, ip_packet, &rohc_packet); 
	if (comp_debug_enable) printf("\n");
	if(status == ROHC_STATUS_OK)
	{
		/* compression is successful */
		if(!rohc_buf_is_empty(rohc_packet))
		{
 			/* rohc_packet.len bytes of compressed IP data available in
			 * rohc_packet: dump the compressed packet on the standard output */
			if (comp_debug_enable) printf("packet resulting from the ROHC compression:\n");
			dump_packet(rohc_packet);

 			for (size_t i = 0; i < rohc_packet.len; ++i) {
				compressed_header_buffer[i] = rohc_buf_byte_at(rohc_packet, i);
			}
			/* to update the size of the compressed header size_t passed in parameter*/
      *comp_header_size =  rohc_packet.len;
		}
		else
		{
			/* no IP packet was compressed because of ROHC segmentation or
			 * feedback-only packet:
			 *  - the ROHC packet was a non-final segment, so at least another
			 *    ROHC segment is required to be able to compress the full
			 *    ROHC packet
			 *  - the ROHC packet was a feedback-only packet, it contained only
			 *    feedback information, so there was nothing to compress */
			if (comp_debug_enable) printf("no packet compressed");
		}
	}
	else
	{
		/* failure: compressor failed to compress the ROHC packet */
		if (comp_debug_enable) printf("\ncompression of fake ROHC packet failed\n");
		
		goto release_compressor;
	}

	if (comp_debug_enable) printf("\nThe program ended successfully.\n");

	return 0;

release_compressor:
	rohc_comp_free(comp_state);
	
	if (comp_debug_enable) printf("\nan error occured during program execution, abort program\n");
	return 1;
}

/**
 * @brief Dump the given network packet on standard output
 *
 * @param packet  The packet to dump
 */
//static void dump_packet(const struct rohc_buf packet)
void RohcCompressorEntity::dump_packet(const struct rohc_buf packet)
{
	size_t i;

	for(i = 0; i < packet.len; ++i)
	{
		if (comp_debug_enable) printf("0x%02x ", rohc_buf_byte_at(packet, i));
		if(i != 0 && ((i + 1) % 8) == 0)
		{
			if (comp_debug_enable) printf("\n");
		}
	}
	if(i != 0 && ((i + 1) % 8) != 0) /* be sure to go to the line */
	{
		if (comp_debug_enable) printf("\n");
	}
}

RohcCompressorEntity::~RohcCompressorEntity() {
	rohc_comp_free(comp_state);
}

} // end namespace

