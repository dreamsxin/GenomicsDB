package org.genomicsdb.spark;

import org.apache.spark.sql.sources.BaseRelation;
import org.apache.spark.sql.sources.TableScan;
import org.apache.spark.sql.types.StructType;
import org.apache.spark.sql.SQLContext;
import org.apache.spark.sql.Row;
import org.apache.spark.sql.RowFactory;
import org.apache.spark.rdd.RDD;
import org.apache.spark.api.java.JavaPairRDD;
import org.apache.hadoop.conf.Configuration;

import org.genomicsdb.reader.FunctionVariantContextToRow;
import htsjdk.variant.variantcontext.VariantContext;
import scala.collection.JavaConverters;
import scala.Function1;
import scala.reflect.ClassTag;

import java.util.Map;
import java.util.stream.Collectors;
import java.util.function.Function;

public class GenomicsDBRelation extends BaseRelation implements TableScan {

  private StructType schema;
  GenomicsDBInput<GenomicsDBInputPartition> input;
  private GenomicsDBConfiguration gdbConf;
  private Map<String, GenomicsDBVidSchema> vMap;
  private SQLContext sqlCon;

  public GenomicsDBRelation(){}

  public GenomicsDBRelation(SQLContext sqlContext, scala.collection.immutable.Map<String,String> parameters){
    setInstance(sqlContext, parameters, null);
  }

  public GenomicsDBRelation(SQLContext sqlContext, StructType struct, scala.collection.immutable.Map<String,String> parameters){
    setInstance(sqlContext, parameters, struct);
  }

  private void setInstance(SQLContext sqlContext, scala.collection.immutable.Map<String,String> parameters, StructType struct){
    this.sqlCon = sqlContext;
    Map<String, String> options = scala.collection.JavaConverters.mapAsJavaMapConverter(parameters).asJava();
    this.gdbConf = new GenomicsDBConfiguration(options);
    GenomicsDBSchemaFactory schemaBuilder =
      new GenomicsDBSchemaFactory(options.get(GenomicsDBConfiguration.LOADERJSON));
    if (schema != null){
      this.schema = schemaBuilder.buildSchemaWithVid(schema.fields());
    }else{
      this.schema = schemaBuilder.defaultSchema();
    }
    this.vMap = schemaBuilder.getVidMap();
    setInput(options);
  }

  private void setInput(Map<String,String> options){
    Long blocksize = new Long(1);
    Long maxblock = Long.MAX_VALUE;
    if (options.get("genomicsdb.minqueryblocksize") != null){
      blocksize = Long.valueOf(options.get("genomicsdb.minqueryblocksize"));
    }
    if (options.get("genomicsdb.maxqueryblocksize") != null){
      maxblock = Long.valueOf(options.get("genomicsdb.maxqueryblocksize"));
    }
    input = new GenomicsDBInput<>(
            this.gdbConf,
            this.schema,
            this.vMap,
            blocksize,
            maxblock,
            GenomicsDBInputPartition.class);
  }

  public StructType schema(){
    return this.schema;
  }

  public boolean needConversion(){
    return true;
  }

  public SQLContext sqlContext(){
    return this.sqlCon;
  }

  @Override
  public RDD<Row> buildScan(){
    Configuration hadoopConf = this.sqlCon.sparkContext().hadoopConfiguration();
    if (this.gdbConf.hasProtoLoader()){
      hadoopConf.set(this.gdbConf.LOADERPB, this.gdbConf.getLoaderPB());
    }else{
      hadoopConf.set(this.gdbConf.LOADERJSON, this.gdbConf.getLoaderJsonFile());
    }
    if (this.gdbConf.hasProtoQuery()){
      hadoopConf.set(this.gdbConf.QUERYPB, this.gdbConf.getQueryPB());
    }else{
      hadoopConf.set(this.gdbConf.QUERYJSON, this.gdbConf.getQueryJsonFile());
    }
    hadoopConf.set(this.gdbConf.MPIHOSTFILE, this.gdbConf.getHostFile());

    RDD<scala.Tuple2<String,VariantContext>> variants;
    variants = this.sqlCon.sparkContext().newAPIHadoopRDD(hadoopConf,
        GenomicsDBInputFormat.class, String.class, VariantContext.class);

    // uses default schema only
    ClassTag<Row> rowTag = scala.reflect.ClassTag$.MODULE$.apply(Row.class);
    Function1<scala.Tuple2<String,VariantContext>, Row> buildRow = new FunctionVariantContextToRow();
    return variants.map(buildRow, rowTag);
  }

}
