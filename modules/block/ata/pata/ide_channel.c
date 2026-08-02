

static uint32_t reg2port(ide_channel_t *channel, uint32_t reg) {
	if (reg <= ATA_REG_STATUS) {
		return channel->base + reg;
	} else if (reg <= ATA_REG_LBA5) {
		return channel->base + reg - 0x06;
	} else if (reg <= ATA_REG_DEVADDRESS) {
		return channel->ctrl + reg - 0x0A;
	} else {
		return channel->bmide + reg - 0xE; // idk
	}
}

static void ide_channel_write(ide_channel_t *channel, uint32_t reg, uint8_t data) {
	// set HOB
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
	}
	if (reg <= ATA_REG_STATUS) {
		resource_write8(channel->base, reg, data);
	} else if (reg <= ATA_REG_LBA5) {
		resource_write8(channel->base, reg - 0x06, data);
	} else if (reg <= ATA_REG_DEVADDRESS) {
		resource_write8(channel->ctrl, reg - 0x0A, data);
	} else {
		resource_write8(channel->bmide, reg - 0xE, data);
	}

	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, channel->nIEN);
	}
}

static uint8_t ide_channel_read(ide_channel_t *channel, uint32_t reg) {
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, 0x80 | channel->nIEN);
	}
	uint8_t data;
	if (reg <= ATA_REG_STATUS) {
		data = resource_read8(channel->base, reg);
	} else if (reg <= ATA_REG_LBA5) {
		data = resource_read8(channel->base, reg - 0x06);
	} else if (reg <= ATA_REG_DEVADDRESS) {
		data = resource_read8(channel->ctrl, reg - 0x0A);
	} else {
		data = resource_read8(channel->bmide, reg - 0xE);
	}
	if (reg > 0x07 && reg < 0x0C) {
		ide_channel_write(channel, ATA_REG_CONTROL, channel->nIEN);
