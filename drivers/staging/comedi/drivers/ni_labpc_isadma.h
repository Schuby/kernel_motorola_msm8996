/*
 * ni_labpc ISA DMA support.
*/

#ifndef _NI_LABPC_ISADMA_H
#define _NI_LABPC_ISADMA_H

#define NI_LABPC_HAVE_ISA_DMA	IS_ENABLED(CONFIG_COMEDI_NI_LABPC_ISADMA)

#if NI_LABPC_HAVE_ISA_DMA

int labpc_init_dma_chan(struct comedi_device *dev, unsigned int dma_chan);
void labpc_free_dma_chan(struct comedi_device *dev);

#else

static inline int labpc_init_dma_chan(struct comedi_device *dev,
				      unsigned int dma_chan)
{
	return -ENOTSUPP;
}

static inline void labpc_free_dma_chan(struct comedi_device *dev)
{
}

#endif

#endif /* _NI_LABPC_ISADMA_H */
