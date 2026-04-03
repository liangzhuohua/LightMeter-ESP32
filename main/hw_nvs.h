#ifndef __HW_NVS_H__
#define __HW_NVS_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* NVS通用读写接口 - 支持存储任意数据 */

/* 初始化NVS
 * 返回值: 0成功, -1失败
 */
int hw_nvs_init(void);

/* 写入布尔值
 * namespace: 命名空间名称(如 "wifi", "ui", "settings" 等)
 * key: 键名
 * value: 要写入的布尔值
 * 返回值: 0成功, -1失败
 */
int hw_nvs_set_bool(const char *namespace, const char *key, bool value);

/* 读取布尔值
 * namespace: 命名空间名称
 * key: 键名
 * value: 输出参数，存储读取的值
 * 返回值: 0成功, -1失败(键不存在或类型不匹配)
 */
int hw_nvs_get_bool(const char *namespace, const char *key, bool *value);

/* 写入整数
 * namespace: 命名空间名称
 * key: 键名
 * value: 要写入的整数值
 * 返回值: 0成功, -1失败
 */
int hw_nvs_set_int(const char *namespace, const char *key, int32_t value);

/* 读取整数
 * namespace: 命名空间名称
 * key: 键名
 * value: 输出参数，存储读取的值
 * 返回值: 0成功, -1失败
 */
int hw_nvs_get_int(const char *namespace, const char *key, int32_t *value);

/* 写入64位整数
 * namespace: 命名空间名称
 * key: 键名
 * value: 要写入的64位整数值
 * 返回值: 0成功, -1失败
 */
int hw_nvs_set_i64(const char *namespace, const char *key, int64_t value);

/* 读取64位整数
 * namespace: 命名空间名称
 * key: 键名
 * value: 输出参数，存储读取的值
 * 返回值: 0成功, -1失败
 */
int hw_nvs_get_i64(const char *namespace, const char *key, int64_t *value);

/* 写入字符串
 * namespace: 命名空间名称
 * key: 键名
 * value: 要写入的字符串
 * 返回值: 0成功, -1失败
 */
int hw_nvs_set_string(const char *namespace, const char *key, const char *value);

/* 读取字符串
 * namespace: 命名空间名称
 * key: 键名
 * value: 输出参数，存储读取的字符串
 * len: 输入输出参数，输入时指定value缓冲区大小，输出时返回实际字符串长度
 * 返回值: 0成功, -1失败
 * 注意: 如果len小于实际字符串长度，数据会被截断
 */
int hw_nvs_get_string(const char *namespace, const char *key, char *value, size_t *len);

/* 写入二进制数据
 * namespace: 命名空间名称
 * key: 键名
 * value: 要写入的数据指针
 * len: 数据长度(字节)
 * 返回值: 0成功, -1失败
 */
int hw_nvs_set_blob(const char *namespace, const char *key, const void *value, size_t len);

/* 读取二进制数据
 * namespace: 命名空间名称
 * key: 键名
 * value: 输出参数，存储读取的数据
 * len: 输入输出参数，输入时指定value缓冲区大小，输出时返回实际数据长度
 * 返回值: 0成功, -1失败
 */
int hw_nvs_get_blob(const char *namespace, const char *key, void *value, size_t *len);

/* 删除指定键
 * namespace: 命名空间名称
 * key: 键名
 * 返回值: 0成功, -1失败
 */
int hw_nvs_erase_key(const char *namespace, const char *key);

/* 清空指定命名空间
 * namespace: 命名空间名称
 * 返回值: 0成功, -1失败
 */
int hw_nvs_erase_namespace(const char *namespace);

/* 检查键是否存在
 * namespace: 命名空间名称
 * key: 键名
 * 返回值: true存在, false不存在
 */
bool hw_nvs_key_exists(const char *namespace, const char *key);

#endif
