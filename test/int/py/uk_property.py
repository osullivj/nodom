import duckdb
con = duckdb.connect()
print("connected")
# Install and load the AWS extension
con.execute("INSTALL httpfs; LOAD httpfs;")
print("httpfs loaded")
con.execute("""
    CREATE TABLE IF NOT EXISTS prices AS
    SELECT * FROM read_csv_auto('https://datasets-documentation.s3.eu-west-3.amazonaws.com/uk-house-prices/csv/house_prices.csv.gz')
""")
print("create table done")
# Verify the data
print(con.execute("SELECT COUNT(*) FROM prices").fetchall())
