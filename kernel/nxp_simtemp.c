#include <linux/init.h>
//WWSS#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/stddef.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/device.h>
#include <linux/platform_device.h> 
#include <linux/pci.h>
#include <linux/module.h>
#include <linux/mutex.h>
//#include "CircularBuffer.h"
#define TIME_INTERVAL 2000 // 2 SECONDS
struct timer_list simtemp_timer;
uint32_t u32Sampling_ms = TIME_INTERVAL; // Sampling interval in milliseconds

struct simtemp_sample {
    __u64 timestamp_ns;   // monotonic timestamp
    __s32 temp_mC;        // milli-degree Celsius (e.g., 44123 = 44.123 °C)
    __u32 flags;          // bit0=NEW_SAMPLE, bit1=THRESHOLD_CROSSED (extend as needed)
} __attribute__((packed));

struct simtemp_state {
    struct miscdevice misc_dev;
    struct timer_list timer;
    //CIRCULAR_BUF(sSimTempBuffer);
};

struct simtemp_sample sSimTempSample;
static struct platform_driver nxp_simtemp_driver ;
static struct platform_device* simtemp_pdev;
#define SIMTEMP_BUFFER_SIZE 100
static struct mutex Lock_SamplingInterval;

/********Sysfs functions**********/
static ssize_t sysfs_SamplingMsShow(struct kobject *kobj,struct kobj_attribute *attr, char *buf);
static ssize_t sysfs_SamplingMsStore(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t count);
struct kobj_attribute sSimTemp_attr = __ATTR(sampling_ms, 0660, sysfs_SamplingMsShow, sysfs_SamplingMsStore);


//CIRCULAR_BUF_DEFINE(sSimTempBuffer,uint32_t,SIMTEMP_BUFFER_SIZE);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Omar Felipe Mendiola Meza");
MODULE_DESCRIPTION("Un driver simple de Hola Mundo para el reto de NXP.");
// ===================================================================================
// Timer
// ===================================================================================
static void simtemp_timer_callback(struct timer_list *timer)
{
    //pr_info("SimTemp: Timer callback function called. Current Temp: %d C\n", sSimTempSample.temp_mC);
    sSimTempSample.temp_mC = get_random_u32_inclusive(0,100);
    mod_timer(timer, jiffies + msecs_to_jiffies(2000)); // Re-arm the timer for another 2 seconds
}

// ===================================================================================
// Misc devide
// ===================================================================================
static int simtemp_open(struct inode *inode, struct file *file)
{
    pr_info("EtX misc device open\n");
    //struct simtemp_state *state = container_of(misc_dev, struct simtemp_state, misc_dev);
    return 0;
}
/*
** This function will be called when we close the Misc Device file
*/
static int simtemp_close(struct inode *inodep, struct file *filp)
{
    pr_info("EtX misc device close\n");
    return 0;
}
/*
** This function will be called when we write the Misc Device file
*/
static ssize_t simtemp_write(struct file *file, const char __user *buf,
               size_t len, loff_t *ppos)
{
    pr_info("EtX misc device write\n");
    
    /* We are not doing anything with this data now */
    
    return len; 
}
 
/*
** This function will be called when we read the Misc Device file
*/
static ssize_t simtemp_read(struct file *filp, char __user *buf,
                    size_t count, loff_t *f_pos)
{
    size_t data_size = sizeof(sSimTempSample.temp_mC); // Tamaño de nuestros datos (4 bytes)
    size_t bytes_to_copy;

    pr_info("EtX misc device read\n");

    /* 1. MANEJAR FIN DE ARCHIVO (EOF) */
    // Si f_pos > 0, significa que ya hemos leído los datos en una llamada anterior.
    if (*f_pos >= data_size) {
        pr_info("SimTemp: EOF, no hay más datos que leer.\n");
        return 0; // Retorna 0 para indicar EOF
    }

    /* 2. CALCULAR CUÁNTOS BYTES COPIAR */
    // Respetar lo que pide el usuario (count) y lo que ya hemos leído (f_pos).
    bytes_to_copy = data_size - *f_pos;
    if (bytes_to_copy > count) {
        bytes_to_copy = count;
    }

    /* 3. VERIFICAR ACCESO (access_ok no es estrictamente necesario,
     * pero es buena práctica) */
    if (!access_ok(buf, bytes_to_copy)) {
        pr_err("SimTemp: Read Access NOT OK\n");
        return -EFAULT; // Retornar error si el acceso falla
    }

    /* 4. COPIAR LOS DATOS Y VERIFICAR EL RESULTADO */
    if (copy_to_user(buf, ((char*)&sSimTempSample.temp_mC) + *f_pos, bytes_to_copy)) {
        pr_err("SimTemp: Falló copy_to_user\n");
        return -EFAULT; // Retornar error si la copia falla
    }

    pr_info("SimTemp: Se copiaron %zu bytes\n", bytes_to_copy);

    /* 5. ACTUALIZAR LA POSICIÓN DEL ARCHIVO */
    *f_pos += bytes_to_copy;

    /* 6. RETORNAR EL NÚMERO DE BYTES COPIADOS */
    return bytes_to_copy; // ¡Este es el retorno de éxito!
}

static const struct file_operations sFileOperation = {
    .owner = THIS_MODULE,
    //.llseek = NULL,
    .read = simtemp_read,
    .write = simtemp_write,
   // .unlocked_ioctl = NULL,
    .open = simtemp_open,
    .release = simtemp_close,
   // .mmap = NULL,
   // .compat_ioctl = NULL,
};



struct miscdevice  simtemp_Miscdevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "simtemp",
    .fops = &sFileOperation, // No operations defined for this simple module
    .mode = 0666
};

// ===================================================================================
// --- Funciones para el atributo 'sampling_ms' ---
// ===================================================================================

ssize_t sysfs_SamplingMsShow(struct kobject *kobj,struct kobj_attribute *attr, char *buf)
{
    pr_info("SimTemp: sysfs_SamplingMsShow called\n");
    return sprintf(buf, "%u\n", u32Sampling_ms);
}

ssize_t sysfs_SamplingMsStore(struct kobject *kobj,struct kobj_attribute *attr,const char *buf, size_t count)
{
    unsigned long val;
    int ret;

    pr_info("SimTemp: sysfs_SamplingMsStore called\n");

    ret = kstrtoul(buf, 10, &val);
    if (ret < 0)
        return ret;

    if (val < 100 || val > 60000) {
        pr_err("SimTemp: Invalid sampling interval. Must be between 100 and 60000 ms.\n");
        return -EINVAL;
    }

    u32Sampling_ms = (uint32_t)val;
    pr_info("SimTemp: Sampling interval set to %u ms\n", u32Sampling_ms);

    // Re-arm the timer with the new interval
    mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(u32Sampling_ms));

    return count;
}

// Se ejecuta cuando haces: cat /sys/.../sampling_ms
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    // 1. Obtenemos el puntero a nuestro estado
   // struct simtemp_state *state = dev_get_drvdata(dev);
    
    // 2. Usamos sysfs_emit para formatear el valor de forma segura
    return sysfs_emit(buf, "%u\n", u32Sampling_ms);
}

// Se ejecuta cuando haces: echo 500 > /sys/.../sampling_ms
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    u32 val;
    int ret;

    // struct simtemp_state *state = dev_get_drvdata(dev);

    // 1. Convertimos el texto del usuario a un número
    ret = kstrtou32(buf, 10, &val);
    if (ret)
        return ret;

    // 2. VALIDACIÓN: Nunca confíes en la entrada del usuario
    if (val < 10 || val > 60000) {
        dev_err(dev, "Valor de sampling_ms fuera de rango (10-60000)\n");
        return -EINVAL; // Invalid argument
    }

    // 3. Bloqueamos, actualizamos el valor y desbloqueamos
   // mutex_lock(&state->lock);
    u32Sampling_ms = val;
   // mutex_unlock(&state->lock);

    // 4. Aplicamos el nuevo valor al timer
    mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(u32Sampling_ms));
    
    dev_info(dev, "Intervalo de muestreo cambiado a %u ms\n", u32Sampling_ms);

    return count;
}



// Crea un atributo de lectura/escritura llamado "sampling_ms" que usa las funciones de arriba
static DEVICE_ATTR_RW(sampling_ms);
// TODO: Haz lo mismo para threshold_mC con DEVICE_ATTR_RW(threshold_mC);

// Crea una lista con todos los atributos que quieres registrar
static struct attribute *simtemp_attrs[] = {
    &dev_attr_sampling_ms.attr,
    // &dev_attr_threshold_mC.attr, // <-- Aquí añadirías más atributos
    NULL, // La lista debe terminar en NULL
};

// Crea el grupo de atributos
static const struct attribute_group simtemp_attr_group = {
    .attrs = simtemp_attrs,
};



// ===================================================================================
// --- module Init and Exit ---
// ===================================================================================

static int __init simtemp_init(void) {
    // printk(KERN_INFO "SimTemp: Hola, Kernel! El modulo ha sido cargado.\n");
    // misc_register(&simtemp_Miscdevice);
    // timer_setup(&simtemp_timer, simtemp_timer_callback, 0);
    // mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(TIME_INTERVAL));

    // /*Create sysfs*/
    // kobj_ref = kobject_create_and_add("simtemp", kernel_kobj);
    // return 0;

    int ret;
    pr_info("SimTemp: Módulo cargado, iniciando registro...\n");

    // 1. REGISTRAR EL DRIVER PRIMERO (el "taxista" se conecta a la app)
    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        pr_err("SimTemp: Falló el registro del platform driver\n");
        return ret;
    }
    pr_info("SimTemp: Platform driver registrado.\n");

    // 2. CREAR Y REGISTRAR UN DISPOSITIVO FALSO (el "pasajero" pide un viaje)
    // Usamos el mismo nombre que el driver para que el kernel los enlace.
    simtemp_pdev = platform_device_alloc("nxp_simtemp", -1);
    if (!simtemp_pdev) {
        ret = -ENOMEM;
        goto err_unregister_driver;
    }

    ret = platform_device_add(simtemp_pdev);
    if (ret) {
        pr_err("SimTemp: Falló al añadir el platform device\n");
        platform_device_put(simtemp_pdev); // Liberar memoria si add falla
        goto err_unregister_driver;
    }
    
    timer_setup(&simtemp_timer, simtemp_timer_callback, 0);
    mod_timer(&simtemp_timer, jiffies + msecs_to_jiffies(TIME_INTERVAL));

    pr_info("SimTemp: Platform device falso creado y añadido.\n");

    return 0;

err_unregister_driver:
    platform_driver_unregister(&nxp_simtemp_driver);
    return ret;
}

static void __exit simtemp_exit(void) {
    // pr_info("SimTemp: Descargando el módulo...\n");
    // // La limpieza debe ser en orden inverso a la creación
    // platform_device_unregister(simtemp_pdev);
    // platform_driver_unregister(&nxp_simtemp_driver);
    // pr_info("SimTemp: Dispositivo y driver desregistrados. Adiós!\n");

    pr_info("SimTemp: Descargando el módulo...\n");
    // La limpieza debe ser en orden inverso a la creación
    timer_delete_sync(&simtemp_timer);
    platform_device_unregister(simtemp_pdev);
    platform_driver_unregister(&nxp_simtemp_driver);
    pr_info("SimTemp: Dispositivo y driver desregistrados. Adiós!\n");
}

static int nxp_simtemp_probe(struct platform_device *pdev)
{
int ret;

    simtemp_Miscdevice.parent = &pdev->dev;
    ret = misc_register(&simtemp_Miscdevice);
    if (ret != 0)
    {
        pr_err("SimTemp: Falló el registro del misc device\n");
        //platform_device_unregister(simtemp_pdev);
        //goto err_unregister_driver;
    }

    ret = sysfs_create_group(&simtemp_Miscdevice.this_device->kobj, &simtemp_attr_group);
    if (ret) {
        misc_deregister(&simtemp_Miscdevice);
        pr_err("SimTemp: Falló al crear el grupo de atributos en sysfs\n");
        return ret;
    }
    printk(KERN_INFO "SimTemp: nxp_simtemp_probe called\n");
    return 0;
}

static int nxp_simtemp_remove(struct platform_device *pdev)
{
     // Limpia el grupo de atributos del kobject del dispositivo
    sysfs_remove_group(&simtemp_Miscdevice.this_device->kobj, &simtemp_attr_group);
    misc_deregister(&simtemp_Miscdevice);

    printk(KERN_INFO "SimTemp: nxp_simtemp_remove called\n");
    return 0;
}

// ===================================================================================
// module Init and Exit
// ===================================================================================

 module_init(simtemp_init);
 module_exit(simtemp_exit);


// ===================================================================================
// 4. REGISTRO DEL DRIVER
// ===================================================================================

static const struct of_device_id nxp_simtemp_of_match[] = {
    { .compatible = "nxp,simtemp" },
    { /* Sentinela */ }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_of_match);

static struct platform_driver nxp_simtemp_driver = {
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
    .driver = {
        .name = "nxp_simtemp",
        .of_match_table = nxp_simtemp_of_match,
    },
};

// Usa la macro de conveniencia para registrar/desregistrar el driver
//module_platform_driver(nxp_simtemp_driver);