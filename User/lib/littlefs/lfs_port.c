#include <lfs_port.h>
#include <assert.h>
#include <stdarg.h>

static sfud_flash_t sfud_dev = NULL;

static int sfud_lfs_read(const struct lfs_config *c, lfs_block_t block, 
                  lfs_off_t off, void *buffer, lfs_size_t size) {
  assert(sfud_dev);
  assert(sfud_dev->init_ok);
  
  if (sfud_read(sfud_dev, block*sfud_dev->chip.erase_gran+off, size, buffer) != SFUD_SUCCESS) {
    return LFS_ERR_IO;
  }
  
  return LFS_ERR_OK;
}


static int sfud_lfs_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size) {
  assert(sfud_dev);
  assert(sfud_dev->init_ok);
  
  if(sfud_write(sfud_dev, block*sfud_dev->chip.erase_gran+off, size, buffer) != SFUD_SUCCESS) {
    return LFS_ERR_IO;
  }
  
  return LFS_ERR_OK;
}


static int sfud_lfs_erase(const struct lfs_config *c, lfs_block_t block) {
  assert(sfud_dev);
  assert(sfud_dev->init_ok);
  
  if(sfud_erase(sfud_dev, block*sfud_dev->chip.erase_gran, sfud_dev->chip.erase_gran) != SFUD_SUCCESS) {
    return LFS_ERR_IO;
  }
  
  return LFS_ERR_OK;
}


static int sfud_lfs_sync(const struct lfs_config *c) {
  return LFS_ERR_OK;
}

struct lfs_config lfs_cfg = {
  .read = sfud_lfs_read,
  .prog = sfud_lfs_prog,
  .erase = sfud_lfs_erase,
  .sync = sfud_lfs_sync,
  
  .read_size = 256,
  .prog_size = 256,
  .block_size = 4096,
  .block_count = 512,
  .cache_size = 512,
  .lookahead_size = 512,
  .block_cycles = 500,
};


int sfud_lfs_init(void) {
  sfud_dev = sfud_get_device(0);
  if (NULL == sfud_dev)
  {
    return -1;
  }
  lfs_cfg.block_size = sfud_dev->chip.erase_gran;
  lfs_cfg.block_count = sfud_dev->chip.capacity/sfud_dev->chip.erase_gran;
  
  printf("[LFS/INFO] LFS block size: %d!\n", lfs_cfg.block_size);
  printf("[LFS/INFO] LFS block count: %d!\n", lfs_cfg.block_count);
  
  return 0;
}


