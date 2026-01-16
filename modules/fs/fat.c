#include <kernel/module.h>
#include <kernel/string.h>
#include <kernel/kheap.h>
#include <kernel/print.h>
#include <kernel/vfs.h>
#include <module/fat.h>
#include <stdint.h>
#include <errno.h>

#undef min
#define min(a,b) (a < b ? a : b)

static vfs_ops_t fat_ops;

static int fat_getattr(vfs_node_t *node,struct stat *st){
	fat_inode *inode = node->private_inode;
	//no meta data on root (emulated on fat 32 root)
	if(inode->is_fat16_root)return 0;

	//TODO : parse times
	st->st_ino  = inode->first_cluster;
	st->st_size = inode->entry.file_size;
	st->st_mode = 0777;
	return 0;
}

static size_t fat_cluster2offset(fat *fat_info,uint32_t cluster){
	return cluster * fat_info->cluster_size + fat_info->data_start;
}

static uint32_t fat_get_next_cluster(fat *fat_info,uint32_t cluster){
	//check oob first
	size_t ent_size; //one unit = half a byte
	switch(fat_info->fat_type){
	case FAT12:
		ent_size = 3;
		break;
	case FAT16:
		ent_size = 4;
		break;
	case FAT32:
		ent_size = 8;
		break;
	}
	if(cluster > fat_info->sectors_per_fat * fat_info->sector_size * 2 / ent_size)return FAT_EOF;
	off_t offset = fat_info->reserved_sectors * fat_info->sector_size + cluster * ent_size / 2;
	uint32_t ent;
	switch(fat_info->fat_type){
	case FAT12:;
		uint16_t ent16;
		vfs_read(fat_info->dev,&ent16,offset,sizeof(ent16));
		ent = ent16 >> (cluster % 2) * 8; //not sure might be the inverse
		if(ent == 0xFFF)ent = 0xFFFFFFFF;
		break;
	case FAT16:
		vfs_read(fat_info->dev,&ent16,offset,sizeof(ent16));
		ent = ent16;
		if(ent == 0xFFFF)ent = 0xFFFFFFFF;
		break;
	case FAT32:
		vfs_read(fat_info->dev,&ent,offset,sizeof(ent));
		break;
	}
	ent &= 0xFFFFFFFF;
	return ent;
}

static vfs_node_t *fat_entry2node(fat_entry *entry,fat *fat_info){
	fat_inode *inode = kmalloc(sizeof(fat_inode));
	memset(inode,0,sizeof(fat_inode));
	inode->fat_info = *fat_info;
	inode->entry    = *entry;
	inode->first_cluster = (entry->cluster_higher << 16) | entry->cluster_lower;

	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
	memset(node,0,sizeof(vfs_node_t));
	node->private_inode = inode;
	node->ops           = &fat_ops;
	if(entry->attribute & ATTR_DIRECTORY){
		node->flags |= VFS_DIR;
	} else {
		node->flags |= VFS_FILE;
	}
	return node;
}

static ssize_t fat_read(vfs_fd_t *fd,void *buf,off_t offset,size_t count){
	fat_inode *inode = fd->private;

	if(offset > inode->entry.file_size){
		return 0;
	} else if(offset + count > inode->entry.file_size){
		count = inode->entry.file_size - offset;
	}

	uint32_t start = offset / inode->fat_info.cluster_size;
	uint32_t end   = (offset + count + inode->fat_info.cluster_size - 1) / inode->fat_info.cluster_size;
	uint32_t cluster = inode->first_cluster;

	//amount of data read
	size_t data_read = 0;
	
	//start by going to the first cluster
	for(size_t i=0; i<start; i++){
		//early EOF ??? probably corrupted fat fs
		if(cluster == FAT_EOF)return -EIO;
		cluster = fat_get_next_cluster(&inode->fat_info,cluster);
	}
	for(size_t i=start; i<end; i++){
		//early EOF ??? probably corrupted fat fs
		if(cluster == FAT_EOF)return -EIO;

		size_t off = fat_cluster2offset(&inode->fat_info,cluster);
		size_t read_size = min(count,inode->fat_info.cluster_size);
		if(i == start){
			//allow unaligned read
			off += offset % inode->fat_info.cluster_size;

			if(read_size + (offset % inode->fat_info.cluster_size) > inode->fat_info.cluster_size){
				read_size = inode->fat_info.cluster_size - (offset % inode->fat_info.cluster_size);
			}
		}


		ssize_t r = vfs_read(inode->fat_info.dev,buf,off,read_size);

		if(r < 0)return r;
		data_read += r;
		if((size_t)r < min(count,inode->fat_info.cluster_size))return data_read;
		buf = (char *)buf + r;
		count -= r;

		cluster = fat_get_next_cluster(&inode->fat_info,cluster);
	}

	return data_read;
}

static int fat_readdir(vfs_fd_t *fd,unsigned long index,struct dirent *dirent){
	fat_inode *inode = fd->private;

	uint64_t offset   = inode->is_fat16_root ? inode->start         : fat_cluster2offset(&inode->fat_info,inode->first_cluster);
	uint32_t remaning = inode->is_fat16_root ? inode->entries_count : UINT32_MAX;
	uint32_t cluster  = inode->first_cluster;
	kdebugf("readdir on %s , first cluster is %lx\n",inode->is_fat16_root ? "root" : "not root",cluster);
	while (index > 0){
		if(!remaning)return -ENOENT;
		//skip everything with VOLUME_ID attr or free
		for(;;){
			fat_entry entry;
			vfs_read(inode->fat_info.dev,&entry,offset,sizeof(entry));
			if(entry.name[0] == 0x00){
				//everything is free after that
				//we hit last
				return -ENOENT;
			}
			if(!(entry.attribute & ATTR_VOLUME_ID) && (entry.name[0] != (char)0xe5))break;
			offset += sizeof(fat_entry);
			if(!inode->is_fat16_root && !(offset % inode->fat_info.cluster_size)){
				//end of cluster
				//jump to next
				cluster = fat_get_next_cluster(&inode->fat_info,cluster);
				if(cluster == FAT_EOF)return -ENOENT;
				offset = fat_cluster2offset(&inode->fat_info,cluster);
			}
			remaning--;
			if(!remaning)return -ENOENT;
		}
		index--;
		offset += sizeof(fat_entry);
		if(!inode->is_fat16_root && !(offset % inode->fat_info.cluster_size)){
			//end of cluster
			//jump to next
			cluster = fat_get_next_cluster(&inode->fat_info,cluster);
			if(cluster == FAT_EOF)return -ENOENT;
			offset = fat_cluster2offset(&inode->fat_info,cluster);
		}
		remaning--;
	}
	if(!remaning)return -ENOENT;

	//skip everything with VOLUME_ID attr
	//TODO : handle long name
	for(;;){
		fat_entry entry;
		vfs_read(inode->fat_info.dev,&entry,offset,sizeof(entry));
		if(entry.name[0] == 0x00){
			//everything is free after that
			//we hit last
			return -ENOENT;
		}
		if(!(entry.attribute & ATTR_VOLUME_ID) && (entry.name[0] != (char)0xe5))break;
		offset += sizeof(fat_entry);
		if(!inode->is_fat16_root && !(offset % inode->fat_info.cluster_size)){
			//end of cluster
			//jump to next
			cluster = fat_get_next_cluster(&inode->fat_info,cluster);
			if(cluster == FAT_EOF)return -ENOENT;
			offset = fat_cluster2offset(&inode->fat_info,cluster);
		}
		remaning--;
		if(!remaning)return -ENOENT;
	}

	//now parse the entry
	fat_entry entry;
	vfs_read(inode->fat_info.dev,&entry,offset,sizeof(entry));
	
	size_t j=0;
	for(int i=0; i<8; i++){
		if(entry.name[i] == ' ')break;
		dirent->d_name[j++] = entry.name[i];
	}
	//don't add . for directories without extention
	if(!((entry.attribute & ATTR_DIRECTORY) && entry.name[8] == ' ')){
		dirent->d_name[j++] = '.';
	}
	for(int i=8; i<11; i++){
		if(entry.name[i] == ' ')break;
		dirent->d_name[j++] = entry.name[i];
	}
	dirent->d_name[j] = '\0';

	if (entry.attribute & ATTR_DIRECTORY) {
		dirent->d_type = DT_DIR;
	} else {
		dirent->d_type = DT_REG;
	}
	return 0;
}

static vfs_node_t *fat_lookup(vfs_node_t *node,const char *name){
	fat_inode *inode = node->private_inode;

	uint32_t remaning = inode->is_fat16_root ? inode->entries_count : UINT32_MAX;
	uint64_t offset   = inode->is_fat16_root ? inode->start         : fat_cluster2offset(&inode->fat_info,inode->first_cluster);
	uint32_t cluster  = inode->first_cluster;

	while(remaning > 0){
		fat_entry entry;
		vfs_read(inode->fat_info.dev,&entry,offset,sizeof(entry));
		offset += sizeof(fat_entry);
		if(!inode->is_fat16_root && !(offset % inode->fat_info.cluster_size)){
			//end of cluster
			//jump to next
			cluster = fat_get_next_cluster(&inode->fat_info,cluster);
			if(cluster == FAT_EOF)return NULL;
			offset = fat_cluster2offset(&inode->fat_info,cluster);
		}
		if(inode->is_fat16_root)remaning--;

		if(entry.name[0] == 0x00){
			//everything is free after that
			//we hit last
			return NULL;
		}
		//TODO : long name support
		if((entry.attribute & ATTR_VOLUME_ID) || (entry.name[0] == (char)0xe5))cont: continue;

		size_t j = 0;
		for(int i=0; i<8; i++){
			if(entry.name[i] == ' ')break;
			//broken entry check
			if(entry.name[i] < 0x20)goto cont;
			if(name[j++] != entry.name[i])goto cont;
		}
		if(name[j] == '.')j++;
		for(int i=8; i<11; i++){
			if(entry.name[i] == ' ')break;
			//broken entry check
			if(entry.name[i] < 0x20)goto cont;
			if(name[j++] != entry.name[i])goto cont;
		}
		if(name[j])continue;

		//we found it
		return fat_entry2node(&entry,&inode->fat_info);
	}

	return NULL;
}

int fat_mount(const char *source,const char *target,unsigned long flags,const void *data){
	(void)flags;
	(void)data;
	vfs_fd_t *dev = vfs_open(source,O_RDONLY);
	if(!dev)return -ENOENT;

	fat_bpb bpb;
	if(vfs_read(dev,&bpb,0,sizeof(bpb)) != sizeof(bpb)){
		vfs_close(dev);
		return -EIO; //not sure this is the good error code
	}
	kdebugf("%ld\n",offsetof(fat_bpb,extended.fat32.signature));
	if(bpb.extended.fat32.signature != 0xaa55){
		kdebugf("invalid signature\n");
		vfs_close(dev);
		return -EFTYPE;
	}

	kdebugf("oem is '%s'\n",bpb.oem_name);

	if(bpb.byte_per_sector < 512 || bpb.sector_per_cluster == 0){
		kdebugf("invalid byte per sectors / sectors per cluster\n");
		vfs_close(dev);
		return -EFTYPE;
	}
	
	uint16_t root_sectors = ((bpb.root_entires_count * 32) + bpb.byte_per_sector - 1) / bpb.byte_per_sector;
	uint32_t sectors_per_fat = bpb.sectors_per_fat16 ? bpb.sectors_per_fat16 : bpb.extended.fat32.sectors_per_fat32;
	uint32_t sectors_count   = bpb.sectors_count16   ? bpb.sectors_count16   : bpb.sectors_count32;
	uint32_t data_sectors    = sectors_count - (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat + root_sectors);
	uint32_t data_clusters   = data_sectors / bpb.sector_per_cluster;

	int fat_type;
	if(data_clusters < 4085){
		fat_type = FAT12;
		kdebugf("fat12\n");
	} else if(data_clusters < 65525){
		fat_type = FAT16;
		kdebugf("fat16\n");
	} else {
		fat_type = FAT32;
		kdebugf("fat32\n");
	}

	if(fat_type == FAT32){
		if(bpb.extended.fat32.version != 0){
			vfs_close(dev);
			return -EFTYPE;
		}
	} else {
		//TODO : fat12/16 check
	}

	fat fat_info = {
		.dev              =  dev,
		.fat_type         = fat_type,
		.reserved_sectors = bpb.reserved_sectors,
		.sector_size      = bpb.byte_per_sector,
		.cluster_size     = bpb.byte_per_sector * bpb.sector_per_cluster,
		.data_start       = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector,
	};
	vfs_node_t *local_root;
	if(fat_type == FAT32){
		//build a fake entry for root
		fat_entry root_entry;
		memset(&root_entry,0,sizeof(root_entry));
		root_entry.attribute = ATTR_DIRECTORY;
		root_entry.cluster_lower  =  bpb.extended.fat32.root_cluster & 0xf;
		root_entry.cluster_higher =  (bpb.extended.fat32.root_cluster >> 8) & 0xf;
		//how do we get size ?
		local_root = fat_entry2node(&root_entry,&fat_info);
	} else {
		//root of fat12/16 is really stupid
		fat_inode *root = kmalloc(sizeof(fat_inode));
		root->fat_info      = fat_info;
		root->entries_count = bpb.root_entires_count;
		root->start         = (bpb.reserved_sectors + bpb.fat_count * sectors_per_fat) * bpb.byte_per_sector;
		root->is_fat16_root = 1;

		local_root = kmalloc(sizeof(vfs_node_t));
		memset(local_root,0,sizeof(vfs_node_t));
		local_root->private_inode = root;
		local_root->flags = VFS_DIR;
		local_root->ops = &fat_ops;
	}

	

	int ret = vfs_mount(target,local_root);
	if(ret < 0){
		kfree(local_root->private_inode);
		kfree(local_root);
		vfs_close(dev);
		return ret;
	}

	return 0;
}

static vfs_ops_t fat_ops = {
	.read    = fat_read,
	.readdir = fat_readdir,
	.lookup  = fat_lookup,
	.getattr = fat_getattr,
};

static vfs_filesystem_t fat_fs = {
	.mount = fat_mount,
	.name = "fat",
};

int fat_init(int argc,char **argv){
	(void)argc;
	(void)argv;
	vfs_register_fs(&fat_fs);
	return 0;
}

int fat_fini(){
	//TODO : make sure no partition is mounted
	vfs_unregister_fs(&fat_fs);
	return 0;
}

kmodule module_meta = {
	.magic = MODULE_MAGIC,
	.init = fat_init,
	.fini = fat_fini,
	.author = "tayoky",
	.name = "fat",
	.description = "fat12/16/32 drivers",
	.license = "GPL 3",
};
